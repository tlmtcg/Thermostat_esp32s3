// ------------------------------
// Variables globales
// ------------------------------
const statusEl = document.getElementById("status");
const tabContent = document.getElementById("tab-content");
let currentConfig = null;

let cityList = ["Roncq", "Dieppe", "Lille", "Paris"];

// ------------------------------
// Fonctions utilitaires
// ------------------------------
function setStatus(msg) {
  if (statusEl) statusEl.textContent = msg;
}

// ------------------------------
// Chargement des onglets
// ------------------------------
function loadTab(name) {
  const file = {
    meteo: "config_meteo.html",
    thermo: "config_thermo.html",
    sht31: "config_sht31.html",
    jeedom: "config_jeedom.html",
  }[name];

  if (!file) return;

  fetch("/" + file)
    .then((r) => r.text())
    .then((html) => {
      tabContent.innerHTML = html;
      applyConfigToUI();

      // Si l’onglet météo est chargé → remplir la liste
      if (document.getElementById("city_select")) {
        loadCityList();
      }
    })
    .catch(() => setStatus("Erreur chargement onglet"));
}

// ------------------------------
// Application de la config dans l’UI
// ------------------------------
function applyConfigToUI() {
  if (!currentConfig) return;

  const cfg = currentConfig;

  // Météo
  if (document.getElementById("city")) {
    city.value = cfg.weather.city;
    lat.value = cfg.weather.lat;
    lon.value = cfg.weather.lon;
  }

  // Thermostat
  if (document.getElementById("th_offset")) {
    th_offset.value = cfg.thermostat.offset;
    th_hyst.value = cfg.thermostat.hysteresis;
    th_auto.checked = cfg.thermostat.auto_mode;
  }

  // SHT31
  if (document.getElementById("sht_tcal")) {
    sht_tcal.value = cfg.sht31.temp_cal;
    sht_hcal.value = cfg.sht31.hum_cal;
  }

  // Jeedom
  if (document.getElementById("jee_en")) {
    jee_en.checked = cfg.jeedom.enabled;
    jee_id.value = cfg.jeedom.id;
  }
}

// ------------------------------
// Chargement de la config depuis l’ESP
// ------------------------------
function loadConfig() {
  fetch("/api/config")
    .then((r) => r.json())
    .then((cfg) => {
      currentConfig = cfg;
      applyConfigToUI();
      setStatus("Config chargée");
    })
    .catch(() => setStatus("Erreur chargement config"));
}

// ------------------------------
// Sauvegarde de la config
// ------------------------------
function saveConfig() {
  if (!currentConfig) currentConfig = {};
  const cfg = currentConfig;

  // Météo
  if (document.getElementById("city")) {
    cfg.weather = cfg.weather || {};
    cfg.weather.city = city.value;
    cfg.weather.lat = parseFloat(lat.value);
    cfg.weather.lon = parseFloat(lon.value);
  }

  // Thermostat
  if (document.getElementById("th_offset")) {
    cfg.thermostat = cfg.thermostat || {};
    cfg.thermostat.offset = parseFloat(th_offset.value);
    cfg.thermostat.hysteresis = parseFloat(th_hyst.value);
    cfg.thermostat.auto_mode = th_auto.checked;
  }

  // SHT31
  if (document.getElementById("sht_tcal")) {
    cfg.sht31 = cfg.sht31 || {};
    cfg.sht31.temp_cal = parseFloat(sht_tcal.value);
    cfg.sht31.hum_cal = parseFloat(sht_hcal.value);
  }

  // Jeedom
  if (document.getElementById("jee_en")) {
    cfg.jeedom = cfg.jeedom || {};
    cfg.jeedom.enabled = jee_en.checked;
    cfg.jeedom.id = parseInt(jee_id.value);
  }

  fetch("/api/config", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(cfg),
  })
    .then((r) => r.json())
    .then(() => setStatus("Configuration sauvegardée ✔"))
    .catch(() => setStatus("Erreur sauvegarde ❌"));
}

// ------------------------------
// Gestion des villes (onglet météo)
// ------------------------------
function loadCityList() {
  const sel = document.getElementById("city_select");
  if (!sel) return;

  sel.innerHTML = "";
  cityList.forEach((c) => {
    const opt = document.createElement("option");
    opt.value = c;
    opt.textContent = c;
    sel.appendChild(opt);
  });
}

function selectCity() {
  const city = city_select.value;

  fetch(`/api/weather/geocode?city=${encodeURIComponent(city)}`)
    .then((r) => r.json())
    .then((j) => {
      // Mise à jour des champs visibles
      lat.value = j.lat;
      lon.value = j.lon;

      // 🔥 Mise à jour de la configuration interne
      currentConfig.weather.city = city;
      currentConfig.weather.lat = j.lat;
      currentConfig.weather.lon = j.lon;

      saveConfig();
      console.log("Ville sélectionnée:", city, "lat:", j.lat, "lon:", j.lon);
    });
}

function addCity() {
  const c = city_new.value.trim();
  if (!c) return;
  cityList.push(c);
  loadCityList();
  city_select.value = c;
  selectCity();
}

function updateWeather() {
  fetch("/api/weather/update", { method: "POST" })
    .then((r) => r.json())
    .then((j) => console.log(j));
}

// ------------------------------
// Initialisation
// ------------------------------
function initTabs() {
  document.querySelectorAll(".tab-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      document
        .querySelectorAll(".tab-btn")
        .forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      loadTab(btn.dataset.tab);
    });
  });

  loadTab("meteo");
  loadConfig();
}

window.onload = initTabs;
