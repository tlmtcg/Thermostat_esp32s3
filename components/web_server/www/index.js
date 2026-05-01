function toggleMenu() {
    document.getElementById('menu').classList.toggle('open');
}

function toggleSub(id, el) {
    document.getElementById(id).classList.toggle('open');
    el.querySelector('.submenu-arrow').classList.toggle('rotate');
}

// Charger le menu commun
document.addEventListener("DOMContentLoaded", async () => {
    try {
        const response = await fetch('/nav.html');
        document.getElementById('navbar').innerHTML = await response.text();
    } catch(err) {
        console.error("Menu non chargé :", err);
    }
});