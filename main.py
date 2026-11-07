"""Змейка по вопросам. Только стандартная библиотека (tkinter)."""
import random
import tkinter as tk

import minigame

DEMO = [
    {"question": "Сколько бит в байте?", "options": ["4", "8", "16", "32"], "answer": 1},
    {"question": "Что такое RAM?", "options": ["Жёсткий диск", "Оперативная память", "Процессор", "Видеокарта"], "answer": 1},
    {"question": "Расширение файла Excel?", "options": [".docx", ".xlsx", ".pptx", ".txt"], "answer": 1},
]

CELL, GRID, SPEED, LIVES = 24, 20, 150, 3
COLORS = ["#ff4d6d", "#4dd2ff", "#ffd24d", "#7dff6a"]

data = minigame.load()


def build_questions(raw):
    """Приложение отдаёт вопросы как {question, answer, answers}: вариантов нет.
    Неверные варианты берём из верных ответов других вопросов."""
    pool = [str(q["answer"]) for q in raw if q.get("answer")]
    out = []
    for q in raw:
        if q.get("options") is not None:               # готовый формат с вариантами (демо)
            out.append(q)
            continue
        right = str(q.get("answer", "")).strip()
        if not right or not q.get("question"):
            continue
        others = list({a for a in pool if a.strip().lower() != right.lower()})
        random.shuffle(others)
        options = [right] + others[:3]
        while len(options) < 2:
            options.append("Нет верного ответа")
        random.shuffle(options)
        out.append({"question": q["question"], "options": options, "answer": options.index(right)})
    return out


questions = build_questions(data.get("tests") or []) or DEMO
random.shuffle(questions)
questions = questions[:10]
TOTAL = len(questions)

root = tk.Tk()
root.title("Змейка по вопросам")
root.resizable(False, False)

label = tk.Label(root, font=("Segoe UI", 12, "bold"), wraplength=GRID * CELL, pady=6)
label.pack()
canvas = tk.Canvas(root, width=GRID * CELL, height=GRID * CELL, bg="#10121a", highlightthickness=0)
canvas.pack()
status = tk.Label(root, font=("Segoe UI", 10), wraplength=GRID * CELL, justify="left")
status.pack()

state = {"snake": [(10, 10), (9, 10), (8, 10)], "dir": (1, 0), "next": (1, 0),
         "q": 0, "score": 0, "lives": LIVES, "foods": {}, "done": None}


def spawn():
    q = questions[state["q"]]
    label.config(text=f"{state['q'] + 1}/{TOTAL}: {q['question']}")
    free = [(x, y) for x in range(GRID) for y in range(GRID) if (x, y) not in state["snake"]]
    cells = random.sample(free, len(q["options"]))
    state["foods"] = {c: i for i, c in enumerate(cells)}


def draw():
    canvas.delete("all")
    for (x, y) in state["snake"]:
        canvas.create_rectangle(x * CELL + 1, y * CELL + 1, (x + 1) * CELL - 1, (y + 1) * CELL - 1,
                                fill="#39ff88", outline="")
    q = questions[state["q"]]
    for (x, y), i in state["foods"].items():
        canvas.create_oval(x * CELL, y * CELL, (x + 1) * CELL, (y + 1) * CELL, fill=COLORS[i % 4], outline="")
        canvas.create_text(x * CELL + CELL // 2, y * CELL + CELL // 2, text="ABCD"[i % 4], font=("Segoe UI", 10, "bold"))
    # варианты ответов под полем
    status.config(text="   ".join(f"{'ABCD'[i % 4]}) {o}" for i, o in enumerate(q["options"]))
                  + f"\nОчки: {state['score']}   Жизни: {state['lives']}")


def finish_game(completed):
    state["done"] = completed
    root.destroy()


def tick():
    s = state
    s["dir"] = s["next"]
    hx, hy = s["snake"][0]
    head = (hx + s["dir"][0], hy + s["dir"][1])
    if not (0 <= head[0] < GRID and 0 <= head[1] < GRID) or head in s["snake"]:
        return finish_game(False)
    s["snake"].insert(0, head)
    if head in s["foods"]:
        if s["foods"][head] == questions[s["q"]]["answer"]:
            s["score"] += 1
        else:
            s["lives"] -= 1
        s["q"] += 1
        if s["lives"] <= 0:
            return finish_game(False)
        if s["q"] >= TOTAL:
            return finish_game(True)
        spawn()
    else:
        s["snake"].pop()
    draw()
    root.after(SPEED, tick)


def on_key(e):
    d = {"Up": (0, -1), "Down": (0, 1), "Left": (-1, 0), "Right": (1, 0),
         "w": (0, -1), "s": (0, 1), "a": (-1, 0), "d": (1, 0)}.get(e.keysym)
    if d and (d[0] + state["dir"][0], d[1] + state["dir"][1]) != (0, 0):
        state["next"] = d


root.bind("<Key>", on_key)
root.protocol("WM_DELETE_WINDOW", lambda: finish_game(False))
spawn()
draw()
root.after(SPEED, tick)
root.mainloop()

minigame.finish(score=state["score"], max=TOTAL, completed=bool(state["done"]))
