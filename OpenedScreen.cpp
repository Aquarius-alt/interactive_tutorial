#include "OpenedScreen.h"
#include "RepoStore.h"
#include "TextButton.h"
#include "Text.h"
static std::vector<UndoHistory> g_answerUndo;   // история Ctrl+Z для каждого ответа
#include "Localization.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <vector>
#include <map>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "Shell32.lib")

namespace {
struct Hit { SDL_FRect rc{}; int type=0, index=-1, sub=-1; };   // sub = номер вложения (type 7/8)
std::string g_dir;
std::vector<TheoryItem> g_theory, g_tests;
std::vector<bool> g_done;
std::vector<std::string> g_answers;
std::vector<bool> g_noteOpen, g_testOpen;
bool g_theoryOpen=true, g_testsOpen=true, g_answerFocus=false, g_dirty=false;
int g_scroll=0, g_answerIndex=-1;
std::vector<Hit> g_hits;
TextButton g_back{20,20,110,38,"BACK"}, g_save{0,0,130,40,"SAVE"};
const Uint8 R=90,G=200,B=255;
std::filesystem::path RepoPath(){ return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(g_dir.data()), g_dir.size())); }
std::string Hex(const std::string& s){ static const char* h="0123456789ABCDEF"; std::string o; for(unsigned char c:s){o+=h[c>>4];o+=h[c&15];} return o; }
std::string Unhex(const std::string& s){ std::string o; for(size_t i=0;i+1<s.size();i+=2){ auto v=[](char c){return c<='9'?c-'0':(c<='F'?c-'A'+10:c-'a'+10);}; o.push_back(char((v(s[i])<<4)|v(s[i+1]))); } return o; }
void Save(){ std::ofstream f(RepoPath()/"progress.dat",std::ios::binary|std::ios::trunc); if(!f)return; for(size_t i=0;i<g_done.size();++i) f<<"N "<<i<<" "<<(g_done[i]?1:0)<<"\n"; for(size_t i=0;i<g_answers.size();++i) f<<"A "<<i<<" "<<(g_answers[i].empty()?"-":Hex(g_answers[i]))<<"\n"; g_dirty=false; }
void Load(){ g_done.assign(g_theory.size(),false); g_answers.assign(g_tests.size(),""); std::ifstream f(RepoPath()/"progress.dat",std::ios::binary); std::string type,val; size_t i; while(f>>type>>i>>val){ if(type=="N"&&i<g_done.size())g_done[i]=(val=="1"); else if(type=="A"&&i<g_answers.size())g_answers[i]=(val=="-"?std::string():Unhex(val)); } }
// OPENED использует тот же Press Start 2P и Unicode-рендеринг Windows (GDI),
// что и остальные экраны приложения.
struct CachedText { SDL_Texture* texture=nullptr; int w=0, h=0; };
std::map<std::string, CachedText> g_textCache;
SDL_Renderer* g_textRenderer=nullptr;
float g_viewW = 0.f;      // ширина окна (задаётся в Opened_Draw): строки длиннее обрезаются с ".."

// Конец строки, если она не влезает (для поля ответа: видно то, что печатаешь сейчас).
std::string TailFit(const std::string& s, float maxW) {
    const size_t fit = static_cast<size_t>(std::max(1.f, std::floor(maxW / 16.f)));
    size_t drop = Utf8Count(s) > fit ? Utf8Count(s) - fit : 0, pos = 0;
    while (pos < s.size() && drop) {
        const unsigned char c = static_cast<unsigned char>(s[pos]);
        pos += c < 0x80 ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4));
        --drop;
    }
    return s.substr(std::min(pos, s.size()));
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (count <= 0) {
        count = MultiByteToWideChar(CP_ACP, 0, value.data(),
            static_cast<int>(value.size()), nullptr, 0);
        if (count <= 0) return L"?";
        std::wstring result(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()),
            result.data(), count);
        return result;
    }
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), count);
    return result;
}

void ClearTextCache() {
    for (auto& item : g_textCache)
        if (item.second.texture) SDL_DestroyTexture(item.second.texture);
    g_textCache.clear();
}

void Text(SDL_Renderer* r, const std::string& sIn, float x, float y,
          float scale=1, Uint8 alpha=255) {
    const std::string s = g_viewW > 0.f
        ? Utf8Fit(sIn, g_viewW - 20.f - x, std::max(8, static_cast<int>(16.0f * scale)))
        : sIn;
    if (s.empty()) return;
    if (g_textRenderer != r) {
        ClearTextCache();
        g_textRenderer = r;
    }
    const int fontHeight = std::max(8, static_cast<int>(16.0f * scale));
    const std::string key = s + "\x1f" + std::to_string(fontHeight) + "\x1f" + std::to_string(alpha);
    auto it = g_textCache.find(key);
    if (it == g_textCache.end()) {
        const std::wstring wide = Utf8ToWide(s);
        HDC dc = CreateCompatibleDC(nullptr);
        if (!dc) return;
        HFONT font = CreateFontW(-fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
            FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"Press Start 2P");
        HGDIOBJ oldFont = SelectObject(dc, font);
        SIZE extent{};
        GetTextExtentPoint32W(dc, wide.c_str(), static_cast<int>(wide.size()), &extent);
        const int tw = (std::max)(1, static_cast<int>(extent.cx) + 4);
        const int th = (std::max)(fontHeight + 5, static_cast<int>(extent.cy) + 4);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = tw;
        info.bmiHeader.biHeight = -th; // top-down DIB
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap || !pixels) {
            if (bitmap) DeleteObject(bitmap);
            SelectObject(dc, oldFont); DeleteObject(font); DeleteDC(dc); return;
        }
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        std::memset(pixels, 0, static_cast<size_t>(tw) * th * 4);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(R, G, B));
        RECT rect{0, 0, tw, th};
        DrawTextW(dc, wide.c_str(), static_cast<int>(wide.size()), &rect,
            DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        // DIB32 содержит BGRA; задаём альфа-канал для SDL.
        auto* bytes = static_cast<Uint8*>(pixels);
        for (int i = 0; i < tw * th; ++i) {
            const Uint8 coverage = bytes[i * 4]; // синий канал содержит интенсивность сглаживания
            bytes[i * 4 + 0] = B;
            bytes[i * 4 + 1] = G;
            bytes[i * 4 + 2] = R;
            bytes[i * 4 + 3] = coverage;
        }
        SDL_Texture* texture = SDL_CreateTexture(r, SDL_PIXELFORMAT_BGRA32,
            SDL_TEXTUREACCESS_STATIC, tw, th);
        if (texture) {
            SDL_UpdateTexture(texture, nullptr, pixels, tw * 4);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(texture, alpha);
        }
        SelectObject(dc, oldBitmap); DeleteObject(bitmap);
        SelectObject(dc, oldFont); DeleteObject(font); DeleteDC(dc);
        if (!texture) return;
        it = g_textCache.emplace(key, CachedText{texture, tw, th}).first;
    }
    SDL_FRect dst{x, y, static_cast<float>(it->second.w), static_cast<float>(it->second.h)};
    SDL_RenderTexture(r, it->second.texture, nullptr, &dst);
}
void Rect(SDL_Renderer*r,SDL_FRect q,bool hot=false){ SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(r,hot?45:22,hot?48:24,hot?58:30,255); SDL_RenderFillRect(r,&q); SDL_SetRenderDrawColor(r,R,G,B,hot?230:110); SDL_RenderRect(r,&q); }
void AddHit(float x,float y,float w,float h,int type,int idx,int sub=-1){g_hits.push_back({{x,y,w,h},type,idx,sub});}
bool In(const SDL_FRect&q,float x,float y){return x>=q.x&&x<=q.x+q.w&&y>=q.y&&y<=q.y+q.h;}

// ---------- просмотр вложений ----------
// Клик по файлу в раскрытом конспекте / тесте: картинка открывается тут же поверх экрана
// (CLOSE / Esc закрывает, OPEN запускает её во внешней программе), любой другой файл
// (pdf, docx, архив, ...) сразу открывается программой Windows по умолчанию.
int g_mx=-1, g_my=-1;
SDL_Surface* g_viewSurf=nullptr; SDL_Texture* g_viewTex=nullptr; SDL_Renderer* g_viewOwner=nullptr;
std::string g_viewPath;
TextButton g_viewClose{0,0,130,38,"CLOSE"}, g_viewExt{0,0,150,38,"OPEN"};
std::string BaseNameOf(const std::string& p){ const size_t k=p.find_last_of("/\\"); return k==std::string::npos?p:p.substr(k+1); }
void FreeView(){ if(g_viewTex)SDL_DestroyTexture(g_viewTex); if(g_viewSurf)SDL_DestroySurface(g_viewSurf); g_viewTex=nullptr; g_viewSurf=nullptr; g_viewOwner=nullptr; g_viewPath.clear(); }
// Исполняемые файлы и скрипты по клику не запускаем: вместо этого показываем их в Проводнике.
bool IsRiskyFile(const std::string& path){
    std::wstring ext=std::filesystem::path(Utf8ToWide(path)).extension().wstring();
    for(wchar_t& c:ext) c=static_cast<wchar_t>(::towlower(c));
    static const wchar_t* risky[]={L".exe",L".com",L".bat",L".cmd",L".scr",L".msi",L".ps1",L".vbs",L".vbe",L".js",L".jse",L".wsf",L".hta",L".lnk",L".jar",L".reg",L".dll"};
    for(const wchar_t* e:risky) if(ext==e) return true;
    return false;
}
void OpenExternal(const std::string& path){
    const std::wstring wpath=Utf8ToWide(path);
    if(IsRiskyFile(path)){
        const std::wstring args=L"/select,\""+wpath+L"\"";
        ShellExecuteW(nullptr,L"open",L"explorer.exe",args.c_str(),nullptr,SW_SHOWNORMAL);
        return;
    }
    ShellExecuteW(nullptr,L"open",wpath.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
}
// ---------- перенос строк и ползунок ----------
// Шрифт моноширинный (16 px на символ), поэтому ширина строки = число символов * 16.
size_t U8Len(unsigned char c){ return c<0x80?1:((c&0xE0)==0xC0?2:((c&0xF0)==0xE0?3:4)); }

// Режет текст на строки не длиннее maxChars символов: по пробелам, а слишком длинное слово - по символам.
// Переводы строк в тексте сохраняются (пустая строка = пустая строка).
std::vector<std::string> WrapText(const std::string& in,size_t maxChars){
    std::vector<std::string> out; if(maxChars<1)maxChars=1;
    std::string text; for(char c:in) if(c!='\r') text.push_back(c);
    size_t p=0;
    while(true){
        const size_t e=text.find('\n',p);
        const std::string para=text.substr(p,e==std::string::npos?std::string::npos:e-p);
        std::vector<size_t> off;
        for(size_t i=0;i<para.size();){ off.push_back(i); i+=std::min(U8Len(static_cast<unsigned char>(para[i])),para.size()-i); }
        off.push_back(para.size());
        const size_t n=off.size()-1; size_t start=0;
        if(n==0) out.push_back(std::string());
        while(start<n){
            if(n-start<=maxChars){ out.push_back(para.substr(off[start])); break; }
            size_t brk=std::string::npos;
            for(size_t k=start+maxChars;k>start;--k) if(para[off[k]]==' '){ brk=k; break; }
            if(brk==std::string::npos){
                out.push_back(para.substr(off[start],off[start+maxChars]-off[start])); start+=maxChars;
            } else {
                std::string ln=para.substr(off[start],off[brk]-off[start]);
                while(!ln.empty()&&ln.back()==' ') ln.pop_back();
                out.push_back(ln); start=brk;
                while(start<n&&para[off[start]]==' ') ++start;
            }
        }
        if(e==std::string::npos) break;
        p=e+1;
    }
    return out;
}
// То же с маркером в начале первой строки; остальные строки сдвинуты на ширину маркера.
std::vector<std::string> WrapPrefixed(const std::string& prefix,const std::string& text,size_t maxChars){
    const size_t pl=Utf8Count(prefix); const size_t inner=maxChars>pl+1?maxChars-pl:1;
    std::vector<std::string> lines=WrapText(text,inner);
    for(size_t i=0;i<lines.size();++i) lines[i]=(i==0?prefix:std::string(pl,' '))+lines[i];
    return lines;
}

float g_viewTopY=80.f, g_viewBotY=0.f;           // область прокрутки (клики вне неё не попадают в содержимое)
int   g_maxScroll=0;
SDL_FRect g_track{0,0,0,0}, g_thumb{0,0,0,0};
bool  g_hasBar=false, g_drag=false; float g_dragOff=0.f;
void ScrollFromMouse(float my){
    const float travel=g_track.h-g_thumb.h; if(travel<=0.f) return;
    const float t=std::clamp((my-g_dragOff-g_track.y)/travel,0.f,1.f);
    g_scroll=std::clamp(int(std::lround(t*float(g_maxScroll))),0,g_maxScroll);
}
void OpenAttachment(const std::string& path){
    FreeView();
    SDL_Surface* sf=SDL_LoadSurface(path.c_str());      // картинка, которую умеет читать SDL (PNG, BMP, ...)
    if(sf){ g_viewSurf=sf; g_viewPath=path; return; }
    OpenExternal(path);                                  // не картинка (или формат не читается): системная программа
}
}
void Opened_Open(SDL_Window* window,const std::string& dir){ g_dir=dir; CreateData d; g_theory.clear();g_tests.clear(); if(RepoStore_Load(dir,d)){g_theory=d.theory;g_tests=d.tests;} g_noteOpen.assign(g_theory.size(),false);g_testOpen.assign(g_tests.size(),false);g_theoryOpen=true;g_testsOpen=true;g_answerFocus=false;g_answerIndex=-1;g_scroll=0;g_answerUndo.clear();g_dirty=false;Load(); SDL_StartTextInput(window); }
void Opened_Close(SDL_Window* window){ FreeView(); g_drag=false; if(g_dirty)Save(); SDL_StopTextInput(window);g_dir.clear();g_theory.clear();g_tests.clear();g_done.clear();g_answers.clear();g_hits.clear();g_answerFocus=false; }
void Opened_Draw(SDL_Renderer*r,int w,int h,SDL_Rect*nd,int&ndCount){
 g_viewW=float(w); g_back.label=UiText("BACK","НАЗАД"); g_save.label=UiText("SAVE","СОХР.");
 g_hits.clear(); Text(r,UiText("OPENED REPOSITORY","ОТКРЫТЫЙ РЕПОЗИТОРИЙ"),20,22,1); Text(r,g_dir.substr(g_dir.find_last_of("/\\")+1),20,48,1,180);
 g_back.x=20.0f;g_back.y=static_cast<float>(h-62);TextButton_Draw(r,g_back);nd[ndCount++]=TextButton_Rect(g_back);
 g_save.x=static_cast<float>(w-188);g_save.y=static_cast<float>(h-62);g_save.hovered=g_dirty;TextButton_Draw(r,g_save);nd[ndCount++]=TextButton_Rect(g_save);

 // Содержимое листается внутри области между заголовком и кнопками. Все строки переносятся по ширине окна,
 // а если всё не помещается по высоте, справа появляется ползунок.
 const float viewTop=80.f, viewBot=std::max(viewTop+40.f,float(h)-70.f);
 g_viewTopY=viewTop; g_viewBotY=viewBot;
 const float x=20.f, width=std::max(120.f,float(w)-40.f-16.f);     // 16 px справа - под ползунок
 const float rowRight=x+width;
 auto charsFor=[&](float from,float to){ return static_cast<size_t>(std::max(1.f,std::floor((to-from)/16.f))); };
 auto vis=[&](float top,float hh){ return top+hh>viewTop&&top<viewBot; };
 float y=0.f;

 // draw=false: только измерить высоту (чтобы поджать прокрутку до рисования), draw=true: рисовать и собирать клики
 auto build=[&](bool draw){
   y=88.f-float(g_scroll);

   auto section=[&](const char* title,bool opened,int type){
     if(draw&&vis(y,38.f)){ SDL_FRect q{x,y,width,38.f}; Rect(r,q); Text(r,std::string(opened?"v  ":">  ")+title,x+12,y+12); AddHit(x,y,width,38.f,type,-1); }
     y+=46.f;
   };
   // строка с названием конспекта / теста: название переносится, рамка растёт
   auto titleRow=[&](bool opened,const std::string& title,int type,int idx){
     const std::vector<std::string> lines=WrapPrefixed(opened?"v  ":">  ",title,charsFor(x+18,rowRight-18));
     const float H=std::max(34.f,float(lines.size())*18.f+16.f);
     if(draw&&vis(y,H)){
       SDL_FRect q{x+8,y,width-16,H}; Rect(r,q);
       for(size_t k=0;k<lines.size();++k) Text(r,lines[k],x+18,y+10.f+float(k)*18.f);
       AddHit(q.x,q.y,q.w,q.h,type,idx);
     }
     y+=H+6.f;
   };
   auto contentLines=[&](const std::string& content){
     const std::vector<std::string> lines=WrapText(content,charsFor(x+28,rowRight-8));
     for(const std::string& ln:lines){ if(draw&&!ln.empty()&&vis(y+4.f,18.f)) Text(r,ln,x+28,y+4.f,1,210); y+=18.f; }
   };
   // вложения: подпись FILES и по строке на файл; клик открывает файл (см. OpenAttachment)
   auto fileList=[&](const std::vector<std::string>&files,int type,int idx){
     if(files.empty())return;
     if(draw&&vis(y,24.f)) Text(r,std::string(UiText("FILES:","ФАЙЛЫ:")),x+28,y+4,1,150);
     y+=24.f;
     for(size_t k=0;k<files.size();++k){
       const std::vector<std::string> lines=WrapPrefixed("> ",BaseNameOf(files[k]),charsFor(x+38,rowRight-36));
       const float H=std::max(28.f,float(lines.size())*18.f+12.f);
       if(draw&&vis(y,H)){
         SDL_FRect q{x+28,y,width-56,H};
         Rect(r,q,In(q,float(g_mx),float(g_my)));
         for(size_t j=0;j<lines.size();++j) Text(r,lines[j],q.x+10,y+6.f+float(j)*18.f);
         AddHit(q.x,q.y,q.w,q.h,type,idx,int(k));
       }
       y+=H+6.f;
     }
     y+=4.f;
   };
   auto doneRow=[&](size_t i){
     const std::string label=g_done[i]?UiText("NOTE COMPLETED","КОНСПЕКТ ИЗУЧЕН"):UiText("MARK AS COMPLETED","ОТМЕТИТЬ КАК ИЗУЧЕННЫЙ");
     const std::vector<std::string> lines=WrapPrefixed(g_done[i]?"[x] ":"[ ] ",label,charsFor(x+38,rowRight-36));
     const float H=std::max(30.f,float(lines.size())*18.f+12.f);
     if(draw&&vis(y,H)){
       SDL_FRect c{x+28,y,width-56,H}; Rect(r,c,g_done[i]);
       for(size_t k=0;k<lines.size();++k) Text(r,lines[k],c.x+10,y+10.f+float(k)*18.f);
       AddHit(c.x,c.y,c.w,c.h,3,int(i));
     }
     y+=H+8.f;
   };
   // поле ответа: введённый текст переносится, поле растёт вниз
   auto answerRow=[&](size_t i){
     const bool empty=g_answers[i].empty();
     const std::vector<std::string> lines=WrapText(empty?std::string(UiText("ENTER YOUR ANSWER...","ВВЕДИТЕ ОТВЕТ...")):g_answers[i],charsFor(x+38,rowRight-36));
     const float H=std::max(38.f,float(lines.size())*18.f+20.f);
     if(draw&&vis(y,H)){
       SDL_FRect a{x+28,y,width-56,H}; Rect(r,a,g_answerIndex==int(i));
       for(size_t k=0;k<lines.size();++k) Text(r,lines[k],a.x+10,y+13.f+float(k)*18.f,1,empty?110:255);
       AddHit(a.x,a.y,a.w,a.h,6,int(i));
     }
     y+=H+8.f;
   };

   section(UiText("NOTES","КОНСПЕКТЫ"),g_theoryOpen,1);
   if(g_theoryOpen) for(size_t i=0;i<g_theory.size();++i){
     titleRow(g_noteOpen[i],g_theory[i].title,2,int(i));
     if(g_noteOpen[i]){ contentLines(g_theory[i].content); fileList(g_theory[i].files,7,int(i)); doneRow(i); }
   }
   y+=8.f; section(UiText("TESTS","ТЕСТЫ"),g_testsOpen,4);
   if(g_testsOpen) for(size_t i=0;i<g_tests.size();++i){
     titleRow(g_testOpen[i],g_tests[i].title,5,int(i));
     if(g_testOpen[i]){ contentLines(g_tests[i].content); fileList(g_tests[i].files,8,int(i)); answerRow(i); }
   }
 };

 build(false);                                                       // измерили полную высоту
 g_maxScroll=std::max(0,int(std::ceil(y+float(g_scroll)+8.f-viewBot)));
 g_scroll=std::clamp(g_scroll,0,g_maxScroll);                        // окно выросло / раздел свернули: не уезжаем за конец
 const SDL_Rect clip{0,int(viewTop),w,int(viewBot-viewTop)};
 SDL_SetRenderClipRect(r,&clip);                                     // содержимое не заходит на заголовок и кнопки
 build(true);
 SDL_SetRenderClipRect(r,nullptr);

 // ползунок: только если содержимое не помещается
 g_hasBar=g_maxScroll>0;
 if(g_hasBar){
   g_track={float(w)-30.f,viewTop,6.f,viewBot-viewTop};
   const float thumbH=std::min(g_track.h,std::max(28.f,g_track.h*g_track.h/(g_track.h+float(g_maxScroll))));
   const float travel=g_track.h-thumbH;
   g_thumb={g_track.x,g_track.y+travel*float(g_scroll)/float(g_maxScroll),g_track.w,thumbH};
   SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
   SDL_SetRenderDrawColor(r,R,G,B,40); SDL_RenderFillRect(r,&g_track);   // дорожка
   const bool hot=g_drag||In(g_thumb,float(g_mx),float(g_my));
   for(int i=3;i>=1;--i){                                                // мягкое свечение вокруг ползунка
     SDL_SetRenderDrawColor(r,R,G,B,Uint8((hot?40:26)*(1.f-i/4.f)));
     const SDL_FRect gl{g_thumb.x-i,g_thumb.y-i,g_thumb.w+2.f*i,g_thumb.h+2.f*i}; SDL_RenderFillRect(r,&gl);
   }
   SDL_SetRenderDrawColor(r,hot?200:R,hot?240:G,255,255); SDL_RenderFillRect(r,&g_thumb);
 } else g_drag=false;
 nd[ndCount++]={20,int(viewTop),w-40,int(viewBot-viewTop)};

 if(g_viewSurf){   // просмотр картинки-вложения поверх всего экрана
   if(g_viewOwner!=r){ if(g_viewTex)SDL_DestroyTexture(g_viewTex); g_viewTex=nullptr; g_viewOwner=r; }
   if(!g_viewTex){
     g_viewTex=SDL_CreateTextureFromSurface(r,g_viewSurf);
     if(!g_viewTex){ const std::string path=g_viewPath; FreeView(); OpenExternal(path); return; }   // слишком большая для текстуры: внешняя программа
   }
   SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(r,0,0,0,215); SDL_RenderFillRect(r,nullptr);
   const float availW=std::max(40.f,float(w)-60.f), availH=std::max(40.f,float(h)-170.f);
   float sc=std::min(availW/float(g_viewSurf->w),availH/float(g_viewSurf->h)); sc=std::min(sc,3.f);
   const float dw=std::max(1.f,std::floor(float(g_viewSurf->w)*sc)), dh=std::max(1.f,std::floor(float(g_viewSurf->h)*sc));
   const SDL_FRect dst{std::floor((float(w)-dw)*0.5f),std::floor(70.f+(availH-dh)*0.5f),dw,dh};
   SDL_RenderTexture(r,g_viewTex,nullptr,&dst);
   SDL_SetRenderDrawColor(r,R,G,B,140); const SDL_FRect fr{dst.x-3.f,dst.y-3.f,dst.w+6.f,dst.h+6.f}; SDL_RenderRect(r,&fr);
   Text(r,BaseNameOf(g_viewPath),20,22,1);
   g_viewClose.label=UiText("CLOSE","ЗАКРЫТЬ"); g_viewExt.label=UiText("OPEN","ОТКРЫТЬ");
   g_viewClose.w=std::max(130.f,Utf8TextWidth(g_viewClose.label,16)+32.f); g_viewExt.w=std::max(130.f,Utf8TextWidth(g_viewExt.label,16)+32.f);
   const float rowW=g_viewClose.w+20.f+g_viewExt.w, by=float(h)-62.f;
   g_viewClose.x=std::floor((float(w)-rowW)*0.5f); g_viewExt.x=g_viewClose.x+g_viewClose.w+20.f; g_viewClose.y=g_viewExt.y=by;
   TextButton_Draw(r,g_viewClose); TextButton_Draw(r,g_viewExt);
   nd[ndCount++]={0,0,w,h};
 }
}
OpenedResult Opened_Handle(SDL_Window*window,const SDL_Event&e){
 if(e.type==SDL_EVENT_MOUSE_MOTION){g_mx=int(e.motion.x);g_my=int(e.motion.y);}
 if(e.type==SDL_EVENT_WINDOW_MOUSE_LEAVE){g_mx=g_my=-1;}
 if(g_viewSurf){   // просмотр картинки модальный: события только ему
   if(e.type==SDL_EVENT_KEY_DOWN&&!e.key.repeat&&e.key.key==SDLK_ESCAPE){FreeView();return OpenedResult::None;}
   if(TextButton_Handle(g_viewClose,e)){FreeView();return OpenedResult::None;}
   if(TextButton_Handle(g_viewExt,e)){OpenExternal(g_viewPath);return OpenedResult::None;}
   return OpenedResult::None;
 }
 if(g_answerFocus&&g_answerIndex>=0&&g_answerIndex<int(g_answers.size())&&(e.type==SDL_EVENT_TEXT_INPUT||e.type==SDL_EVENT_KEY_DOWN)){
  std::string&s=g_answers[size_t(g_answerIndex)];
  if(g_answerUndo.size()<g_answers.size())g_answerUndo.resize(g_answers.size());
  UndoHistory&u=g_answerUndo[size_t(g_answerIndex)];
  if(e.type==SDL_EVENT_TEXT_INPUT){UndoBefore(u,s,1);Utf8AppendLimited(s,e.text.text,2000);UndoCommit(u,s);g_dirty=true;return OpenedResult::None;}
  if(IsPasteEvent(e)){UndoBefore(u,s,3);Utf8PasteLimited(s,2000,false);UndoCommit(u,s);g_dirty=true;return OpenedResult::None;}
  if(e.key.key==SDLK_BACKSPACE){UndoBefore(u,s,2);Utf8PopBack(s);UndoCommit(u,s);g_dirty=true;return OpenedResult::None;}
  if(IsCopyEvent(e)){CopyText(s);return OpenedResult::None;}
  if(IsUndoEvent(e)){if(UndoApply(u,s))g_dirty=true;return OpenedResult::None;}
 }
 if(e.type==SDL_EVENT_MOUSE_WHEEL){g_scroll=std::clamp(g_scroll-int(e.wheel.y*40),0,g_maxScroll);return OpenedResult::None;}
 if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&e.button.button==SDL_BUTTON_LEFT&&g_hasBar){   // ползунок: схватить / клик по дорожке
   const SDL_FRect hit{g_track.x-8.f,g_track.y,g_track.w+16.f,g_track.h};
   if(In(hit,e.button.x,e.button.y)){
     const SDL_FRect onThumb{hit.x,g_thumb.y,hit.w,g_thumb.h};
     g_drag=true; g_dragOff=In(onThumb,e.button.x,e.button.y)?e.button.y-g_thumb.y:g_thumb.h*0.5f;
     ScrollFromMouse(e.button.y); return OpenedResult::None;
   }
 }
 if(e.type==SDL_EVENT_MOUSE_MOTION&&g_drag){ScrollFromMouse(e.motion.y);return OpenedResult::None;}
 if(e.type==SDL_EVENT_MOUSE_BUTTON_UP&&e.button.button==SDL_BUTTON_LEFT)g_drag=false;
 if(TextButton_Handle(g_back,e)){Opened_Close(window);return OpenedResult::Back;}
 if(TextButton_Handle(g_save,e)){Save();return OpenedResult::None;}
 if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&e.button.button==SDL_BUTTON_LEFT){for(const Hit&h:g_hits)if(In(h.rc,e.button.x,e.button.y)&&e.button.y>=g_viewTopY&&e.button.y<=g_viewBotY){switch(h.type){case 1:g_theoryOpen=!g_theoryOpen;break;case 2:g_noteOpen[size_t(h.index)]=!g_noteOpen[size_t(h.index)];break;case 3:g_done[size_t(h.index)]=!g_done[size_t(h.index)];g_dirty=true;break;case 4:g_testsOpen=!g_testsOpen;break;case 5:g_testOpen[size_t(h.index)]=!g_testOpen[size_t(h.index)];break;case 6:g_answerFocus=true;g_answerIndex=h.index;break;case 7:{g_answerFocus=false;const auto&fl=g_theory[size_t(h.index)].files;if(h.sub>=0&&size_t(h.sub)<fl.size())OpenAttachment(fl[size_t(h.sub)]);break;}case 8:{g_answerFocus=false;const auto&fl=g_tests[size_t(h.index)].files;if(h.sub>=0&&size_t(h.sub)<fl.size())OpenAttachment(fl[size_t(h.sub)]);break;}}return OpenedResult::None;}g_answerFocus=false;}
 if(e.type==SDL_EVENT_KEY_DOWN&&!e.key.repeat&&e.key.key==SDLK_ESCAPE){Opened_Close(window);return OpenedResult::Back;}
 return OpenedResult::None;
}
bool Opened_IsViewing(){return g_viewSurf!=nullptr;}
void Opened_Destroy(){FreeView();TextButton_Destroy(g_viewClose);TextButton_Destroy(g_viewExt);TextButton_Destroy(g_back);TextButton_Destroy(g_save);ClearTextCache();g_textRenderer=nullptr;}
