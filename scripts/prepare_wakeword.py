from pathlib import Path

Import("env")

main_cpp = Path(env.subst("$PROJECT_DIR")) / "main" / "main.cpp"
text = main_cpp.read_text(encoding="utf-8")

replacements = {
    'wn9_hiesp': 'wn9_alexa',
    'WN9_HIESP': 'WN9_ALEXA',
    'Hi, ESP': 'Alexa',
    'HI, ESP': 'ALEXA',
    'Hi ESP': 'Alexa',
    'HI ESP': 'ALEXA',
}

changed = False
for old, new in replacements.items():
    if old in text:
        text = text.replace(old, new)
        changed = True

if not changed and 'wn9_alexa' not in text:
    raise RuntimeError(
        f"Wake-word preparation found no Hi ESP markers in {main_cpp}; refusing to build an unknown state."
    )

main_cpp.write_text(text, encoding="utf-8")
print("Wake-word preparation: main.cpp configured for wn9_alexa")
