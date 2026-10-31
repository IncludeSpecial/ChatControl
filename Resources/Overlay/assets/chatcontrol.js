// Общий код виджетов ChatControl: опрос мода Satisfactory и названия эффектов.
// Виджеты открываются из самого мода (http://localhost:8787/obs/...), поэтому данные берутся с того же адреса: /state.

const CC_EFFECTS = {
  hi: "Привет", gift: "Подарок", biggift: "Большой подарок", rain: "Ящик с припасами", sink: "Купоны AWESOME Sink",
  jump: "Супер-прыжок", moon: "Лунная гравитация", heavy: "Тяжесть", speed: "Ускорение", slow: "Замедление",
  fov: "Кривая камера", teleport: "Телепорт", hub: "Домой к ХАБу",
  strike: "Забастовка", underclock: "Замедление машин", blackout: "Блэкаут",
  night: "Ночь", day: "День", sign: "Табличка", pet: "Питомец",
  ambush: "Засада", boss: "БОСС",
  fly: "Полёт", shrink: "Малыш", giant: "Великан", hungry: "Голод", inventory: "Бардак в инвентаре", drop: "Вещи из карманов",
  fog: "Туман", dance: "Заморозка", heal: "Лечение", name: "Подпись машины",
};
// Текст из чата вставляется в страницу только через ccEsc
const ccEsc = s => String(s).replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c]);
const ccName = id => ccEsc(CC_EFFECTS[id] || id);

// Вызывает onState(state) раз в interval мс; onState(null), если игра не отвечает
function ccPoll(onState, interval = 1000) {
  async function tick() {
    try {
      const r = await fetch("/state", { cache: "no-store" });
      onState(await r.json());
    } catch { onState(null); }
    setTimeout(tick, interval);
  }
  tick();
}

// Масштаб через ?k=1.4 в адресе, как у остальных виджетов
function ccZoom(el) {
  const k = parseFloat(new URLSearchParams(location.search).get("k")) || 1;
  if (k !== 1) el.style.zoom = k;
}
