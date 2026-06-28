String getWebPage() {
    int remainingSeconds = 0;
    if (isSleepTimerActive) {
        unsigned long elapsed = millis() - sleepTimerStart;
        if (elapsed < sleepTimerDuration) {
            remainingSeconds = (sleepTimerDuration - elapsed) / 1000UL;
        }
    }

    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover\">";
    html += "<title>ESP32 Rádio</title>";
    
    html += "<link href=\"https://fonts.googleapis.com/css2?family=Dancing+Script:wght@700&display=swap\" rel=\"stylesheet\">";
    
    html += "<style>";
    html += "html, body { height: 100%; margin: 0; padding: 0; background-color: #121212; color: #ffffff; text-align: center; font-family: 'Segoe UI', Arial, sans-serif; }";
    html += ".app-layout { display: flex; flex-direction: column; height: 100%; max-width: 800px; margin: 0 auto; }";
    
    html += ".top-panel { flex-shrink: 0; padding: 15px; border-bottom: 2px solid #333; background: #1a1a1a; z-index: 10; }";
    html += ".stations-panel { flex: 1; overflow-y: auto; padding: 15px; }";
    html += ".bottom-panel { flex-shrink: 0; background: #151515; border-top: 2px solid #333; padding: 10px 15px; padding-bottom: max(10px, env(safe-area-inset-bottom)); z-index: 10; }";
    
    html += ".header-title { color: #FF0000; font-family: 'Dancing Script', cursive; font-size: 28px; font-weight: 700; letter-spacing: 1px; margin: 0 0 5px 0; text-shadow: 1px 1px 2px #550000, 0 0 10px rgba(255,0,0,0.5); white-space: nowrap; overflow: hidden; }";
    html += ".header-version { color: #777; font-size: 12px; margin-bottom: 15px; display: flex; justify-content: center; align-items: center; gap: 12px; }";
    html += ".controls-row { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; width: 100%; max-width: 400px; margin: 0 auto 10px auto; }";
    
    html += ".ctrl-btn { background-color: #333333; color: white; border: none; padding: 8px 5px; border-radius: 8px; font-size: 13px; font-weight: bold; cursor: pointer; transition: background-color 0.3s; touch-action: manipulation; }";
    
    html += ".volume-box { display: flex; align-items: center; gap: 10px; max-width: 500px; margin: 0 auto; }";
    html += ".vol-label { font-size: 14px; font-weight: bold; color: #fff; width: 35px; text-align: left; }";
    html += ".vol-btn { background: #2a2a2a; color: #00bcd4; border: 1px solid #444; border-radius: 6px; width: 36px; height: 32px; font-size: 18px; font-weight: bold; cursor: pointer; display: flex; align-items: center; justify-content: center; touch-action: manipulation; flex-shrink: 0; }";
    html += ".vol-btn:active { background: #555; }";
    html += ".vol-slider { flex-grow: 1; height: 6px !important; background: #444; border-radius: 4px; -webkit-appearance: none; margin: 0; }";
    html += ".vol-percent { width: 40px; text-align: right; color: #00bcd4; font-weight: bold; font-size: 14px; }";
    
    html += ".sound-panel { display: none; background: #222; padding: 10px 15px; border-radius: 8px; margin-bottom: 10px; border: 1px solid #333; text-align: left; }";
    html += ".slider-group { margin-bottom: 10px; }"; 
    html += ".slider-group:last-child { margin-bottom: 0; }";
    html += ".slider-group label { display: block; color: #00bcd4; margin-bottom: 2px; font-size: 12px; font-weight: bold; }";
    html += "input[type=range] { width: 100%; height: 6px; background: #444; border-radius: 3px; -webkit-appearance: none; margin-bottom: 3px; }";
    html += "input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; width: 18px; height: 18px; background: #00bcd4; border-radius: 50%; }";
    html += ".scale-labels { display: flex; justify-content: space-between; font-size: 10px; color: #777; padding: 0 2px; }";
    
    html += ".now-playing { background: #000; padding: 15px; border-radius: 8px; margin: 0; border: 1px solid #222; }";
    html += ".now-playing-label { color: #888; font-size: 12px; text-transform: uppercase; margin-bottom: 5px; }";
    
    html += ".station-name { font-size: 22px; font-weight: bold; margin: 0; transition: color 0.3s ease; }";
    html += ".song-title { font-size: 16px; margin-top: 5px; word-wrap: break-word; min-height: 20px; transition: color 0.3s ease; }";
    
    html += ".grid-container { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }";
    html += ".btn-station { background-color: #2a2a2a; border: none; border-radius: 8px; padding: 8px; min-height: 70px; color: white; cursor: pointer; transition: 0.2s; touch-action: manipulation; }";
    html += ".btn-active { border: 2px solid #00bcd4; background-color: #111; }";
    html += ".btn-station img { max-width: 90%; max-height: 50px; }";
    
    // --- SKRYTÍ SCROLLBARU PRO PANEL STANIC ---
    html += ".stations-panel::-webkit-scrollbar { display: none; }";
    html += ".stations-panel { -ms-overflow-style: none; scrollbar-width: none; }";
    
    html += "</style>";
    
    html += "<script>";
    html += "let ws;";
    html += "let sleepInterval;";
    
    html += "function initWS() {";
    html += "  ws = new WebSocket('ws://' + location.hostname + ':81/');";
    
    html += "  ws.onopen = function() { document.getElementById('ws-status').style.color = '#00FF00'; };";
    
    html += "  ws.onmessage = function(e) {";
    html += "    let msg = e.data;";
    html += "    if(msg.startsWith('POWER:')) {";
    html += "      let btn = document.getElementById('btn-power');";
    html += "      if(msg.substring(6) === '1') { ";
    html += "         btn.style.backgroundColor = '#4CAF50'; btn.innerText = 'Rádio hraje'; ";
    html += "         document.getElementById('st-name').style.color = '#00FF00'; ";
    html += "         document.getElementById('s-title').style.color = '#00bcd4'; ";
    html += "      } else { ";
    html += "         btn.style.backgroundColor = '#d32f2f'; btn.innerText = 'Vypnuto'; ";
    html += "         document.getElementById('st-name').style.color = '#666666'; ";
    html += "         document.getElementById('s-title').style.color = '#666666'; ";
    html += "      }";
    html += "    } else if(msg.startsWith('VOL:')) {";
    html += "      let v = msg.substring(4);";
    html += "      document.getElementById('val-vol').innerText = v + '%';";
    html += "      document.getElementById('vol-slider').value = v;";
    html += "    } else if(msg.startsWith('STATION:')) {";
    html += "      document.getElementById('st-name').innerText = msg.substring(8);";
    html += "    } else if(msg.startsWith('SONG:')) {";
    html += "      document.getElementById('s-title').innerText = msg.substring(5);";
    html += "    } else if(msg.startsWith('SLEEP:')) {";
    html += "      let m = parseInt(msg.substring(6));";
    html += "      startSleepCountdown(m * 60);";
    html += "    } else if(msg.startsWith('OUT:')) {";
    html += "      let mode = msg.substring(4); let o = document.getElementById('out-status');";
    html += "      if(mode === 'MUTE') { o.style.color = '#FF0000'; o.innerHTML = '<svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"11 5 6 9 2 9 2 15 6 15 11 19 11 5\"></polygon><line x1=\"23\" y1=\"9\" x2=\"17\" y2=\"15\"></line><line x1=\"17\" y1=\"9\" x2=\"23\" y2=\"15\"></line></svg>'; }";
    html += "      else if(mode === 'BT') { o.style.color = '#0088ff'; o.innerHTML = '<svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"6.5 6.5 17.5 17.5 12 23 12 1 17.5 6.5 6.5 17.5\"></polyline></svg>'; }";
    html += "      else { o.style.color = '#4CAF50'; o.innerHTML = '<svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"11 5 6 9 2 9 2 15 6 15 11 19 11 5\"></polygon><path d=\"M19.07 4.93a10 10 0 0 1 0 14.14M15.54 8.46a5 5 0 0 1 0 7.07\"></path></svg>'; }";
    html += "    }";
    html += "  };";
    
    html += "  ws.onclose = function() { document.getElementById('ws-status').style.color = '#FF0000'; setTimeout(initWS, 2000); };";
    html += "}";

    html += "function sendWS(cmd) { if(ws && ws.readyState === 1) ws.send(cmd); }";
    
    html += "function togglePower() { sendWS('POWER'); }";
    html += "function setVol(v) { sendWS('VOL:' + v); }"; 
    html += "function setEQ() { let l = document.getElementById('eq-low').value; let m = document.getElementById('eq-mid').value; let h = document.getElementById('eq-high').value; sendWS('EQ:'+l+':'+m+':'+h); }";
    html += "function setSleep(m) { sendWS('SLEEP:' + m); }";
    
    html += "function changeStation(id) {";
    html += "  document.getElementById('s-title').innerText = '';";
    html += "  let btns = document.getElementsByClassName('btn-station');";
    html += "  for(let i=0; i<btns.length; i++) btns[i].classList.remove('btn-active');";
    html += "  document.getElementById('btn-st-' + id).classList.add('btn-active');";
    html += "  sendWS('SET:' + id);";
    html += "}";

    html += "function toggleSound(btn) { let el = document.getElementById('sound-panel'); if (el.style.display === 'none' || el.style.display === '') { el.style.display = 'block'; btn.innerText = 'Ekvalizér ▲'; } else { el.style.display = 'none'; btn.innerText = 'Ekvalizér ▼'; } }";

    html += "function changeVolBy(step) {";
    html += "  let s = document.getElementById('vol-slider');";
    html += "  let v = parseInt(s.value) + step;";
    html += "  if (v > 100) v = 100;";
    html += "  if (v < 0) v = 0;";
    html += "  s.value = v;";
    html += "  document.getElementById('val-vol').innerText = v + '%';";
    html += "  setVol(v);"; 
    html += "}";

    html += "function startSleepCountdown(sec) {";
    html += "  clearInterval(sleepInterval);";
    html += "  let el = document.getElementById('sleep-opt');";
    html += "  let sel = document.getElementById('sleep-select');";
    html += "  if(sec > 0) {";
    html += "    let rem = sec;";
    html += "    sleepInterval = setInterval(() => {";
    html += "      rem--;";
    html += "      if(rem > 0) {";
    html += "        let m = Math.floor(rem/60)+1;";
    html += "        if(el && sel.getAttribute('data-open') !== '1') {";
    html += "          el.innerText = m + ' MIN ▼';";
    html += "        }";
    html += "      } else {";
    html += "        clearInterval(sleepInterval);";
    html += "        if(el) el.innerText = 'Časovač ▼'; sel.value = '0';";
    html += "      }";
    html += "    }, 1000);";
    html += "    let m = Math.floor(sec/60)+1;";
    html += "    if(el && sel.getAttribute('data-open') !== '1') el.innerText = m + ' MIN ▼';";
    html += "    sel.value = '';";
    html += "  } else {";
    html += "    if(el) el.innerText = 'Časovač ▼'; sel.value = '0';";
    html += "  }";
    html += "}";

    html += "window.onload = function() {";
    html += "  initWS();";
    if (remainingSeconds > 0) {
        html += "  startSleepCountdown(" + String(remainingSeconds) + ");";
    }
    html += "};";
    html += "</script>";
    
    html += "</head><body><div class=\"app-layout\">";
    
    // ================== HORNÍ PANEL (Fixní) ==================
    html += "<div class=\"top-panel\">";
    
    html += "<div class=\"header-title\">WEBRADIO GEMINI</div>";
    
    html += "<div class=\"header-version\">";
    html += "<span>Ver fw: " + String(fwVersion) + "</span>";
    
    // --- VÝPOČET IKONY PŘI PRVNÍM NAČTENÍ STRÁNKY ---
    String outColor = isMuted ? "#FF0000" : (rezimSluchatka ? "#0088ff" : "#4CAF50");
    String outSvg = "";
    if (isMuted) outSvg = "<svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"11 5 6 9 2 9 2 15 6 15 11 19 11 5\"></polygon><line x1=\"23\" y1=\"9\" x2=\"17\" y2=\"15\"></line><line x1=\"17\" y1=\"9\" x2=\"23\" y2=\"15\"></line></svg>";
    else if (rezimSluchatka) outSvg = "<svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"6.5 6.5 17.5 17.5 12 23 12 1 17.5 6.5 6.5 17.5\"></polyline></svg>";
    else outSvg = "<svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"11 5 6 9 2 9 2 15 6 15 11 19 11 5\"></polygon><path d=\"M19.07 4.93a10 10 0 0 1 0 14.14M15.54 8.46a5 5 0 0 1 0 7.07\"></path></svg>";
    
    html += "<span id=\"out-status\" style=\"color:" + outColor + "; display:flex; align-items:center; transition: color 0.3s;\" title=\"Stav Výstupu\">" + outSvg + "</span>";

    // Původní zelené rádiové spojení
    html += "<span id=\"ws-status\" style=\"color:#FF0000; display:flex; align-items:center; transition: color 0.3s;\" title=\"Spojení s rádiem\">";
    html += "<svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">";
    html += "<rect x=\"2\" y=\"8\" width=\"20\" height=\"12\" rx=\"2\" ry=\"2\"></rect>";
    html += "<line x1=\"2\" y1=\"12\" x2=\"22\" y2=\"12\"></line>";
    html += "<circle cx=\"8\" cy=\"17\" r=\"1.5\"></circle>";
    html += "<line x1=\"18\" y1=\"8\" x2=\"12\" y2=\"2\"></line>"; 
    html += "</svg></span></div>";
    
    html += "<div class=\"controls-row\">";
    html += "<button id=\"btn-power\" class=\"ctrl-btn\" onclick=\"togglePower()\" style=\"background-color:" + String(isRadioPlaying ? "#4CAF50" : "#d32f2f") + "\">" + String(isRadioPlaying ? "Rádio hraje" : "Vypnuto") + "</button>";
    
    html += "<select id=\"sleep-select\" class=\"ctrl-btn\" onchange=\"setSleep(this.value); this.blur();\" onfocus=\"this.setAttribute('data-open', '1'); this.options[0].text = this.options[0].text.replace('▼', '▲');\" onblur=\"this.removeAttribute('data-open'); this.options[0].text = this.options[0].text.replace('▲', '▼');\">";
    if (isSleepTimerActive && currentSleepValue > 0) {
        int remMin = remainingSeconds / 60 + 1;
        html += "<option value=\"\" selected disabled hidden id=\"sleep-opt\">" + String(remMin) + " MIN ▼</option>";
    } else {
        html += "<option value=\"\" selected disabled hidden id=\"sleep-opt\">Časovač ▼</option>";
    }
    html += "<option value=\"0\">Casovac OFF</option><option value=\"1\">1 MIN</option><option value=\"15\">15 MIN</option><option value=\"30\">30 MIN</option><option value=\"60\">60 MIN</option><option value=\"90\">90 MIN</option><option value=\"120\">120 MIN</option></select>";
    html += "<button class=\"ctrl-btn\" onclick=\"toggleSound(this)\">Ekvalizér ▼</button>";
    html += "</div>";
    
    html += "<div id=\"sound-panel\" class=\"sound-panel\">";
    html += "<div class=\"slider-group\"><label>Basy</label><input type=\"range\" id=\"eq-low\" min=\"-20\" max=\"6\" value=\"" + String(eqLow) + "\" onchange=\"setEQ()\"><div class=\"scale-labels\"><span>-20</span><span>-7</span><span>0</span><span>+6</span></div></div>";
    html += "<div class=\"slider-group\"><label>Středy</label><input type=\"range\" id=\"eq-mid\" min=\"-20\" max=\"6\" value=\"" + String(eqMid) + "\" onchange=\"setEQ()\"><div class=\"scale-labels\"><span>-20</span><span>-7</span><span>0</span><span>+6</span></div></div>";
    html += "<div class=\"slider-group\"><label>Výšky</label><input type=\"range\" id=\"eq-high\" min=\"-20\" max=\"6\" value=\"" + String(eqHigh) + "\" onchange=\"setEQ()\"><div class=\"scale-labels\"><span>-20</span><span>-7</span><span>0</span><span>+6</span></div></div>";
    html += "</div>";

    html += "<div class=\"now-playing\">";
    html += "<div class=\"now-playing-label\">Právě hraje:</div>";
    
    html += "<div class=\"station-name\" id=\"st-name\" style=\"color:" + String(isRadioPlaying ? "#00FF00" : "#666666") + ";\">" + String(stationNames[currentStation]) + "</div>";
    html += "<div class=\"song-title\" id=\"s-title\" style=\"color:" + String(isRadioPlaying ? "#00bcd4" : "#666666") + ";\">" + String(aktualniPisnicka) + "</div>";
    
    html += "</div>";
    
    html += "</div>"; // Konec top-panelu
    
    // ================== STŘEDNÍ PANEL (Rolovací stanice) ==================
    html += "<div class=\"stations-panel\"><div class=\"grid-container\">";
    for(int i=0; i<stationCount; i++) {
        String active = (i == currentStation) ? " btn-active" : "";
        html += "<button id=\"btn-st-" + String(i) + "\" class=\"btn-station" + active + "\" onclick=\"changeStation(" + String(i) + ")\">";
        
        if(String(stationLogosWeb[i]).length() > 5) {
            html += "<img src=\"" + String(stationLogosWeb[i]) + "\" onerror=\"this.style.display='none'; this.nextSibling.style.display='block';\">";
            html += "<div style=\"display:none;\">" + String(stationNames[i]) + "</div>";
        } else {
            html += "<div>" + String(stationNames[i]) + "</div>";
        }
        
        html += "</button>";
    }
    html += "</div></div>"; 
    
    // ================== SPODNÍ PANEL (Fixní hlasitost) ==================
    html += "<div class=\"bottom-panel\">";
    html += "<div class=\"volume-box\">";
    html += "<div class=\"vol-label\">VOL:</div>";
    html += "<button class=\"vol-btn\" onclick=\"changeVolBy(-2)\">-</button>";
    html += "<input type=\"range\" id=\"vol-slider\" class=\"vol-slider\" min=\"0\" max=\"100\" value=\"" + String(curVolume) + "\" oninput=\"document.getElementById('val-vol').innerText = this.value + '%'\" onchange=\"setVol(this.value)\">";
    html += "<button class=\"vol-btn\" onclick=\"changeVolBy(2)\">+</button>";
    html += "<div id=\"val-vol\" class=\"vol-percent\">" + String(curVolume) + "%</div>";
    html += "</div></div>"; 

    html += "</div></body></html>";
    return html;
}
