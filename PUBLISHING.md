# Jak nahrát tento projekt na GitHub

Tento balíček je připravený jako kompletní Git repozitář. Stačí ho nahrát na tvůj GitHub účet. Níže máš dvě varianty — přes web (jednodušší) a přes příkazovou řádku (rychlejší pro budoucí změny).

---

## Varianta A: Přes webové rozhraní GitHubu (bez příkazové řádky)

1. Přihlas se na [github.com](https://github.com)
2. Vpravo nahoře **+ → New repository**
3. Vyplň:
   - **Repository name:** `water-monitor`
   - **Description:** `ESP8266 monitoring průtoku a hladiny vody s MQTT a Home Assistant`
   - **Public** nebo **Private** (dle preference)
   - **NEZAŠKRTÁVEJ** "Add a README" (už ho máš)
4. Klikni **Create repository**
5. Na další stránce klikni **uploading an existing file**
6. Přetáhni všechny soubory a složky z této složky (`water-monitor/`)
   - GitHub zachová strukturu složek pokud přetáhneš celé složky
7. Dole **Commit changes**

---

## Varianta B: Přes Git CLI (doporučeno)

### Předpoklady
- Nainstalovaný [Git](https://git-scm.com/)
- GitHub účet + [SSH klíč](https://docs.github.com/en/authentication/connecting-to-github-with-ssh) nebo [Personal Access Token](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens)

### Kroky

```bash
# 1. Přejdi do složky projektu
cd water-monitor

# 2. Inicializuj Git
git init
git branch -M main

# 3. Přidej všechny soubory
git add .
git commit -m "Initial commit: Water Monitor v2.0"

# 4. Vytvoř repozitář na GitHubu (přes web, viz Varianta A kroky 1-4)
#    a pak připoj remote (nahraď USERNAME svým jménem):
git remote add origin git@github.com:USERNAME/water-monitor.git
#    nebo přes HTTPS:
#    git remote add origin https://github.com/USERNAME/water-monitor.git

# 5. Nahraj
git push -u origin main
```

### Pro budoucí změny

```bash
git add .
git commit -m "Popis změny"
git push
```

---

## Doporučená nastavení repozitáře (po nahrání)

### Topics (štítky pro vyhledávání)
V repozitáři klikni na ozubené kolo vedle "About" a přidej topics:
```
esp8266 arduino mqtt home-assistant water-meter flow-sensor
ultrasonic-sensor iot rainwater wemos-d1-mini jsn-sr04t
```

### About sekce
- **Description:** ESP8266 monitoring průtoku a hladiny vody s MQTT a Home Assistant
- **Website:** (volitelné)

### Náhledy diagramů
GitHub neumí renderovat HTML diagramy přímo v README. Pokud chceš, aby byly vidět:
1. Otevři `docs/wiring_fritzing.html` v prohlížeči
2. Vytiskni do PDF nebo udělej screenshot
3. Ulož jako `docs/wiring.png`
4. V README odkaž: `![Wiring](docs/wiring.png)`

---

## Tipy pro veřejný projekt

- **Releases:** Po odladění vytvoř release (Tags → Create release) s verzí `v2.0` a přilož zkompilovaný `firmware.bin` pro snadné OTA
- **Issues:** Zapni je pro zpětnou vazbu
- **Wiki:** Můžeš sem přesunout detailní návody, pokud README naroste
- **GitHub Pages:** Můžeš publikovat `docs/` diagramy jako web (Settings → Pages → source `docs/`)
