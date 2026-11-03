// Shared code of the built-in ChatControl widgets: polling the mod and labels in the mod's language.
// The widgets are opened from the mod itself (http://localhost:8787/obs/...), so the data comes from the same address: /state.

const CC_TEXT = {
  en: {
    vote: "CHAT VOTE", seconds: "s", pot: "CHAT POT", potFor: "for",
    hint: "!pot <chips> to chip in · !chips balance · !effects what you can do",
  },
  ru: {
    vote: "ГОЛОСОВАНИЕ ЧАТА", seconds: "с", pot: "КОПИЛКА ЧАТА", potFor: "на",
    hint: "!pot <чипы> скинуться · !chips баланс · !effects что можно",
  },
};

const CC_EFFECTS = {
  en: {
    hi: "Hello", gift: "Gift", biggift: "Big gift", rain: "Supply crate", sink: "AWESOME Sink coupons",
    jump: "Super jump", moon: "Moon gravity", heavy: "Heavy gravity", speed: "Fast-forward", slow: "Slow motion",
    fov: "Camera warp", teleport: "Teleport", hub: "Back to the HUB",
    strike: "Machine strike", underclock: "Underclock", blackout: "Blackout",
    night: "Night", day: "Day", sign: "Sign", pet: "Pet", ambush: "Ambush", boss: "BOSS", titan: "TITAN",
    fly: "Flight", shrink: "Tiny", giant: "Giant", hungry: "Hunger", inventory: "Inventory chaos", drop: "Lost pockets",
    fog: "Fog", dance: "Freeze creatures", heal: "Heal", name: "Named machine",
  },
  ru: {
    hi: "Привет", gift: "Подарок", biggift: "Большой подарок", rain: "Ящик с припасами", sink: "Купоны AWESOME Sink",
    jump: "Супер-прыжок", moon: "Лунная гравитация", heavy: "Тяжесть", speed: "Ускорение", slow: "Замедление",
    fov: "Кривая камера", teleport: "Телепорт", hub: "Домой к ХАБу",
    strike: "Забастовка", underclock: "Замедление машин", blackout: "Блэкаут",
    night: "Ночь", day: "День", sign: "Табличка", pet: "Питомец", ambush: "Засада", boss: "БОСС", titan: "ТИТАН",
    fly: "Полёт", shrink: "Малыш", giant: "Великан", hungry: "Голод", inventory: "Бардак в инвентаре", drop: "Вещи из карманов",
    fog: "Туман", dance: "Заморозка", heal: "Лечение", name: "Подпись машины",
  },
};

let ccLang = "en";
const ccT = key => CC_TEXT[ccLang][key];

// Text from chat goes into the page only through ccEsc
const ccEsc = s => String(s).replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c]);
const ccName = id => ccEsc(CC_EFFECTS[ccLang][id] || id);

// Calls onState(state) every interval ms; onState(null) when the game does not answer
function ccPoll(onState, interval = 1000) {
  async function tick() {
    try {
      const r = await fetch("/state", { cache: "no-store" });
      const state = await r.json();
      ccLang = state.language === "ru" ? "ru" : "en";
      onState(state);
    } catch { onState(null); }
    setTimeout(tick, interval);
  }
  tick();
}

// ?k=1.4 in the URL makes a widget bigger
function ccZoom(el) {
  const k = parseFloat(new URLSearchParams(location.search).get("k")) || 1;
  if (k !== 1) el.style.zoom = k;
}
