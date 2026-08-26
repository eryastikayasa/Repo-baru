from pathlib import Path

Import("env")

main_cpp = Path(env.subst("$PROJECT_DIR")) / "main" / "main.cpp"
text = main_cpp.read_text(encoding="utf-8")

# feature/oled must contain the Alexa source directly.
# Do not rewrite the source during build; fail early if an old Hi ESP
# marker is still present so the branch cannot silently mix wake words.
if 'wn9_alexa' not in text:
    raise RuntimeError(
        f"Wake-word validation failed: {main_cpp} is not configured for wn9_alexa."
    )

legacy_markers = (
    'wn9_hiesp',
    'WN9_HIESP',
    'Hi, ESP',
    'HI, ESP',
    'Hi ESP',
    'HI ESP',
)
found_legacy = [marker for marker in legacy_markers if marker in text]
if found_legacy:
    raise RuntimeError(
        f"Wake-word validation failed: legacy Hi ESP markers remain in {main_cpp}: {found_legacy}"
    )

print("Wake-word validation: feature/oled is configured for wn9_alexa")
