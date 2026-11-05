#include "OpenedScreen.h"
#include "RepoStore.h"
#include "TextButton.h"
#include "Text.h"
#include "Localization.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <vector>
#include <map>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#pragma comment(lib, "Gdi32.lib")

namespace {
struct Hit { SDL_FRect rc{}; int type=0, index=-1; };
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

void Text(SDL_Renderer* r, const std::string& s, float x, float y,
          float scale=1, Uint8 alpha=255) {
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
void AddHit(float x,float y,float w,float h,int type,int idx){g_hits.push_back({{x,y,w,h},type,idx});}
bool In(const SDL_FRect&q,float x,float y){return x>=q.x&&x<=q.x+q.w&&y>=q.y&&y<=q.y+q.h;}
}
void Opened_Open(SDL_Window* window,const std::string& dir){ g_dir=dir; CreateData d; g_theory.clear();g_tests.clear(); if(RepoStore_Load(dir,d)){g_theory=d.theory;g_tests=d.tests;} g_noteOpen.assign(g_theory.size(),false);g_testOpen.assign(g_tests.size(),false);g_theoryOpen=true;g_testsOpen=true;g_answerFocus=false;g_answerIndex=-1;g_scroll=0;g_dirty=false;Load(); SDL_StartTextInput(window); }
void Opened_Close(SDL_Window* window){ if(g_dirty)Save(); SDL_StopTextInput(window);g_dir.clear();g_theory.clear();g_tests.clear();g_done.clear();g_answers.clear();g_hits.clear();g_answerFocus=false; }
void Opened_Draw(SDL_Renderer*r,int w,int h,SDL_Rect*nd,int&ndCount){
 g_back.label=UiText("BACK","НАЗАД"); g_save.label=UiText("SAVE","СОХР.");
 g_hits.clear(); Text(r,UiText("OPENED REPOSITORY","ОТКРЫТЫЙ РЕПОЗИТОРИЙ"),20,22,1); Text(r,g_dir.substr(g_dir.find_last_of("/\\")+1),20,48,1,180);
 g_back.x=20.0f;g_back.y=static_cast<float>(h-62);TextButton_Draw(r,g_back);nd[ndCount++]=TextButton_Rect(g_back);
 g_save.x=static_cast<float>(w-150);g_save.y=static_cast<float>(h-62);g_save.hovered=g_dirty;TextButton_Draw(r,g_save);nd[ndCount++]=TextButton_Rect(g_save);
 float y=88.0f-float(g_scroll); const float x=20, width=float(w)-40;
 auto section=[&](const char* title,bool&opened,int type){ SDL_FRect q{x,y,width,38};Rect(r,q);Text(r,std::string(opened?"v  ":">  ")+title,x+12,y+12);AddHit(x,y,width,38,type,-1);y+=46;};
 section(UiText("NOTES","КОНСПЕКТЫ"),g_theoryOpen,1);
 if(g_theoryOpen) for(size_t i=0;i<g_theory.size();++i){ SDL_FRect q{x+8,y,width-16,34};Rect(r,q);Text(r,std::string(g_noteOpen[i]?"v  ":">  ")+g_theory[i].title,x+18,y+10);AddHit(q.x,q.y,q.w,q.h,2,int(i));y+=40;
  if(g_noteOpen[i]){ std::string content=g_theory[i].content; size_t p=0; do{size_t e=content.find('\n',p);std::string line=content.substr(p,e==std::string::npos?e:e-p);Text(r,line,x+28,y+4,1,210);y+=18;if(e==std::string::npos)break;p=e+1;}while(y<h-80); SDL_FRect c{x+28,y,width-56,30};Rect(r,c,g_done[i]);Text(r,g_done[i]?UiText("[x] NOTE COMPLETED","[x] КОНСПЕКТ ИЗУЧЕН"):UiText("[ ] MARK AS COMPLETED","[ ] ОТМЕТИТЬ КАК ИЗУЧЕННЫЙ"),c.x+10,y+10);AddHit(c.x,c.y,c.w,c.h,3,int(i));y+=38; }
 }
 y+=8;section(UiText("TESTS","ТЕСТЫ"),g_testsOpen,4);
 if(g_testsOpen) for(size_t i=0;i<g_tests.size();++i){ SDL_FRect q{x+8,y,width-16,34};Rect(r,q);Text(r,std::string(g_testOpen[i]?"v  ":">  ")+g_tests[i].title,x+18,y+10);AddHit(q.x,q.y,q.w,q.h,5,int(i));y+=40;
  if(g_testOpen[i]){std::string content=g_tests[i].content;size_t p=0;do{size_t e=content.find('\n',p);Text(r,content.substr(p,e==std::string::npos?e:e-p),x+28,y+4,1,210);y+=18;if(e==std::string::npos)break;p=e+1;}while(y<h-80);SDL_FRect a{x+28,y,width-56,38};Rect(r,a,g_answerIndex==int(i));Text(r,g_answers[i].empty()?UiText("ENTER YOUR ANSWER...","ВВЕДИТЕ ОТВЕТ..."):g_answers[i],a.x+10,y+13,1,g_answers[i].empty()?110:255);AddHit(a.x,a.y,a.w,a.h,6,int(i));y+=46;}
 }
 nd[ndCount++]={20,80,w-40,h-150};
}
OpenedResult Opened_Handle(SDL_Window*window,const SDL_Event&e){
 if(g_answerFocus&&e.type==SDL_EVENT_TEXT_INPUT&&g_answerIndex>=0){Utf8AppendLimited(g_answers[size_t(g_answerIndex)], e.text.text, 2000);g_dirty=true;return OpenedResult::None;}
 if(g_answerFocus&&e.type==SDL_EVENT_KEY_DOWN&&e.key.key==SDLK_BACKSPACE&&g_answerIndex>=0){auto&s=g_answers[size_t(g_answerIndex)];Utf8PopBack(s);g_dirty=true;return OpenedResult::None;}
 if(e.type==SDL_EVENT_MOUSE_WHEEL){g_scroll=std::max(0,g_scroll-int(e.wheel.y*40));return OpenedResult::None;}
 if(TextButton_Handle(g_back,e)){Opened_Close(window);return OpenedResult::Back;}
 if(TextButton_Handle(g_save,e)){Save();return OpenedResult::None;}
 if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&e.button.button==SDL_BUTTON_LEFT){for(const Hit&h:g_hits)if(In(h.rc,e.button.x,e.button.y)){switch(h.type){case 1:g_theoryOpen=!g_theoryOpen;break;case 2:g_noteOpen[size_t(h.index)]=!g_noteOpen[size_t(h.index)];break;case 3:g_done[size_t(h.index)]=!g_done[size_t(h.index)];g_dirty=true;break;case 4:g_testsOpen=!g_testsOpen;break;case 5:g_testOpen[size_t(h.index)]=!g_testOpen[size_t(h.index)];break;case 6:g_answerFocus=true;g_answerIndex=h.index;break;}return OpenedResult::None;}g_answerFocus=false;}
 if(e.type==SDL_EVENT_KEY_DOWN&&!e.key.repeat&&e.key.key==SDLK_ESCAPE){Opened_Close(window);return OpenedResult::Back;}
 return OpenedResult::None;
}
void Opened_Destroy(){TextButton_Destroy(g_back);TextButton_Destroy(g_save);ClearTextCache();g_textRenderer=nullptr;}
