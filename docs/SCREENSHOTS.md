# Screenshots for README

The repository includes HTML mockups you can use to generate screenshots for the README and documentation.

## Available mockups

| File | What it shows |
|---|---|
| `web_ui_mockup.html` | Web interface dashboard with demo data |
| `wiring_diagram.html` | Schematic wiring diagram |
| `wiring_fritzing.html` | Pictorial (fritzing-style) wiring diagram |

## How to create screenshots

### Option 1: Browser screenshot (quick)
1. Open the HTML file in a browser
2. Use your OS screenshot tool (Win+Shift+S / Cmd+Shift+4)
3. Save as PNG into this `docs/` folder

### Option 2: Full-page capture (recommended for diagrams)
1. Open the HTML file in Chrome/Edge
2. Open DevTools (F12)
3. Ctrl+Shift+P → type "screenshot" → "Capture full size screenshot"
4. Save the PNG

### Option 3: Headless (automated)
```bash
# Requires Node.js + puppeteer
npx puppeteer screenshot web_ui_mockup.html web_ui.png --full-page
```

## Embedding in README

Once you have PNGs, reference them in `README.md`:

```markdown
## Screenshots

### Web interface
![Web UI](docs/web_ui.png)

### Wiring
![Wiring diagram](docs/wiring.png)
```

## Suggested screenshots to capture

- `web_ui.png` — the dashboard tab with the tank gauge and flow metrics
- `wiring.png` — the fritzing-style main panel diagram
- `ha_dashboard.png` — your actual Home Assistant dashboard once it's running (best taken from a live system)
