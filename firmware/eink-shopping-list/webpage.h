#pragma once

// Single-file PWA: everyday items, ad-hoc add/check off, live list, and an
// offline queue so adding an item away from home WiFi isn't just lost.
// "Add to Home Screen" from mobile Safari/Chrome uses manifest.json below
// to get an app-like icon + no browser chrome.

const char PAGE_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Shopping List</title>
<link rel="manifest" href="/manifest.json">
<style>
  body { font-family: -apple-system, sans-serif; margin: 0; padding: 16px;
         background: #f4f4f2; }
  h1 { font-size: 1.3em; margin: 0 0 4px; }
  h2 { font-size: 0.95em; color: #666; margin: 20px 0 8px; text-transform: uppercase;
       letter-spacing: 0.03em; }
  form { display: flex; gap: 8px; margin-bottom: 8px; }
  input[type=text] { flex: 1; padding: 12px; font-size: 1em;
                      border: 1px solid #ccc; border-radius: 8px; }
  button { padding: 12px 16px; font-size: 1em; border: none;
           border-radius: 8px; background: #222; color: white; }
  ul { list-style: none; padding: 0; margin: 0; }
  li { display: flex; align-items: center; gap: 10px; padding: 12px;
       background: white; margin-bottom: 8px; border-radius: 8px; }
  li.done span.text, li.needed span.text { text-decoration: line-through; color: #999; }
  li.needed span.text { text-decoration: none; font-weight: 600; color: #222; }
  li.pending { background: #fff8e6; border: 1px dashed #d8b34a; }
  li.pending span.text { font-style: italic; }
  li .who { margin-left: auto; font-size: 0.8em; color: #999; }
  li button.del { background: #c0392b; padding: 6px 10px; margin-left: auto; }
  li.favorite button.del { background: transparent; color: #bbb; font-size: 0.8em; }
  .toolbar { display: flex; gap: 8px; margin-top: 16px; }
  .toolbar button { flex: 1; background: #555; }
  .favAddRow { display: flex; gap: 8px; margin-top: 8px; }
  .favAddRow input { flex: 1; padding: 8px; font-size: 0.9em; }
  .favAddRow button { padding: 8px 12px; font-size: 0.9em; background: #888; }
  #copyBox { display: none; position: fixed; inset: 0; background: rgba(0,0,0,0.5);
             padding: 24px 16px; box-sizing: border-box; }
  #copyBox .panel { background: white; border-radius: 12px; padding: 16px; }
  #copyBox p { margin: 0 0 10px; font-size: 0.95em; }
  #copyBox textarea { width: 100%; box-sizing: border-box; height: 45vh; font-size: 16px;
                      padding: 10px; border: 1px solid #ccc; border-radius: 8px; }
  #copyBox button { width: 100%; margin-top: 10px; }
  .settings input { width: 100%; box-sizing: border-box; padding: 10px; font-size: 1em;
                    border: 1px solid #ccc; border-radius: 8px; margin-bottom: 8px; }
  .settings button { width: 100%; }
  .rcpt + .rcpt { border-top: 1px solid #e3e3e3; padding-top: 10px; }
  .rcptRow { display: flex; gap: 8px; }
  .rcptRow input:first-child { flex: 0 0 32%; }
  .hint { font-size: 0.85em; color: #666; margin: 4px 0 8px; }
  .emailRow { flex-wrap: wrap; }
  .emailRow button { flex: 1 1 40%; }  /* two per line, however many people */
  .emailRow:empty { display: none; }
  #sendStatus:empty { display: none; }
  #offlineBanner { display: none; background: #fff3cd; color: #7a5b00; padding: 10px 12px;
                   border-radius: 8px; font-size: 0.85em; margin-bottom: 12px; }
</style>
</head>
<body>
<h1>Shopping List</h1>
<div id="whoLabel" style="font-size: 0.8em; color: #999; margin-bottom: 12px;"></div>
<div id="offlineBanner"></div>

<h2>Everyday items</h2>
<ul id="favList"></ul>
<div class="favAddRow">
  <input type="text" id="favText" placeholder="add a recurring item..." autocomplete="off" maxlength="40">
  <button onclick="addFavorite()">+</button>
</div>

<h2>Also need</h2>
<form id="addForm">
  <input type="text" id="itemText" placeholder="anything else..." autocomplete="off" required maxlength="40">
  <button type="submit">Add</button>
</form>
<ul id="list"></ul>

<div class="toolbar">
  <button onclick="exportList()">Copy list for shopping</button>
</div>
<div id="emailButtons" class="toolbar emailRow"></div>
<div id="sendStatus" class="hint"></div>
<div class="toolbar">
  <button onclick="clearAlsoNeed()">Clear 'Also need'</button>
  <button onclick="clearEverything()">Clear everything</button>
</div>

<h2>Sending the list</h2>
<div class="settings">
  <p class="hint">Up to 3 people. Give each an email address, a mobile number, or both.<br>
    <b>Email:</b> the board's <b>Send</b> button and the <b>Email list to...</b>
    buttons. With more than one person, the board asks who to send it to.<br>
    <b>Text:</b> the <b>Text list to...</b> buttons open your phone's Messages
    app with the list ready to send, from your own number. No account needed.</p>
  <div class="rcpt"><div class="rcptRow"><input type="text" id="rName0" placeholder="Name" maxlength="16">
    <input type="email" id="rEmail0" placeholder="email address"></div>
    <input type="tel" id="rPhone0" placeholder="mobile, for texts (optional)"></div>
  <div class="rcpt"><div class="rcptRow"><input type="text" id="rName1" placeholder="Name" maxlength="16">
    <input type="email" id="rEmail1" placeholder="email address (optional)"></div>
    <input type="tel" id="rPhone1" placeholder="mobile, for texts (optional)"></div>
  <div class="rcpt"><div class="rcptRow"><input type="text" id="rName2" placeholder="Name" maxlength="16">
    <input type="email" id="rEmail2" placeholder="email address (optional)"></div>
    <input type="tel" id="rPhone2" placeholder="mobile, for texts (optional)"></div>

  <p class="hint">Sent <b>from</b> a Gmail account using an <b>app password</b>
    (Google Account &gt; Security &gt; 2-Step Verification &gt; App passwords).
    It's stored on the board and can also read that mailbox, so a separate Gmail
    account just for the board is safest.</p>
  <input type="email" id="mailFrom" placeholder="Send from (Gmail address)">
  <input type="password" id="mailPass" placeholder="Gmail app password" autocomplete="new-password">
  <button onclick="saveEmailSettings()">Save</button>
  <div id="mailStatus" class="hint"></div>
</div>

<h2>Settings</h2>
<div class="toolbar">
  <button onclick="resetConfig()" style="background: #999;">Re-run WiFi setup</button>
</div>
<div id="fwVersion" class="hint" style="text-align:center; margin-top:16px;"></div>

<div id="copyBox">
  <div class="panel">
    <p><b>Couldn't copy automatically.</b> Press and hold in the box below,
       choose <b>Select All</b>, then <b>Copy</b>, and paste into Notes.
       Nothing has been cleared from the board.</p>
    <textarea id="copyText"></textarea>
    <button onclick="document.getElementById('copyBox').style.display='none'">Close</button>
  </div>
</div>

<script>
// --- Who's adding: ask once, remember on this phone forever after. No
// typing per item - just a one-time name so items show who added them. ---
function getWho() {
  return localStorage.getItem('who') || '';
}

function ensureWho() {
  if (getWho()) return;
  const name = prompt("What's your name? It's shown next to items you add, and you'll only be asked once on this phone.");
  if (name && name.trim()) {
    localStorage.setItem('who', name.trim().slice(0, 16));  // board keeps 16 max
  }
  renderWhoLabel();
}

function changeWho() {
  const name = prompt('Change your name:', getWho());
  if (name !== null) {
    localStorage.setItem('who', name.trim().slice(0, 16));
    renderWhoLabel();
  }
}

function renderWhoLabel() {
  const label = document.getElementById('whoLabel');
  const who = getWho();
  label.innerHTML = who
    ? `Adding as <strong>${escapeHtml(who)}</strong> (<a href="#" onclick="changeWho(); return false;" style="color: #999;">change</a>)`
    : `<a href="#" onclick="changeWho(); return false;" style="color: #999;">Set your name</a>`;
}

// --- Offline queue: covers the board being briefly unreachable while you're
// at home with this page open - it restarting, reconnecting after a router
// blip, or cycling its setup portal. Items added then are kept on this
// phone and sent automatically once the board answers again.
// (It can't help away from home: this page is served BY the board, so it
// won't even load off the home WiFi.)
//
// Each queued item gets its own qid, and the queue is always re-read from
// localStorage right before changing it, so a flush in progress can never
// overwrite an item added meanwhile by writing back a stale copy.
function getQueue() {
  return JSON.parse(localStorage.getItem('pendingAdds') || '[]');
}
function setQueue(q) {
  localStorage.setItem('pendingAdds', JSON.stringify(q));
}
function enqueue(text, addedBy) {
  const q = getQueue();
  q.push({qid: Date.now().toString(36) + Math.random().toString(36).slice(2, 7), text, addedBy});
  setQueue(q);
}
function dequeue(qid) {
  setQueue(getQueue().filter(q => q.qid !== qid));
}

// Returns true if the board ACCEPTED or REJECTED the item (either way it
// should leave the queue), false only if the board couldn't be reached -
// a rejected item must not sit in the queue forever.
async function postAdd(text, addedBy) {
  let res;
  try {
    res = await fetch('/api/add', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({text, addedBy})
    });
  } catch (e) {
    return false;  // network error: board unreachable
  }
  if (res.status === 409) {
    alert('The board list is full (20 items). Clear some items first. "' + text + '" was not added.');
  } else if (!res.ok) {
    console.warn('Board rejected item', text, res.status);
  }
  return true;
}

let flushing = false;
async function flushQueue() {
  if (flushing) return;  // one flush at a time, or both would post the same items
  flushing = true;
  try {
    for (const item of getQueue()) {
      if (!(await postAdd(item.text, item.addedBy))) break;  // still offline: stop, retry later
      dequeue(item.qid);  // saved immediately, so a reload mid-flush can't re-send it
    }
  } finally {
    flushing = false;
  }
}

function updateOfflineBanner(reachable) {
  const queue = getQueue();
  const banner = document.getElementById('offlineBanner');
  if (!reachable) {
    banner.style.display = 'block';
    banner.textContent = queue.length > 0
      ? `Can't reach the board right now. ${queue.length} item${queue.length > 1 ? 's' : ''} saved on this phone, sent automatically when it's back. Keep this page open.`
      : "Can't reach the board right now. Items you add are saved on this phone and sent when it's back. Keep this page open.";
  } else if (queue.length > 0) {
    banner.style.display = 'block';
    banner.textContent = `Syncing ${queue.length} queued item${queue.length > 1 ? 's' : ''}...`;
  } else {
    banner.style.display = 'none';
  }
}

// Only one refresh runs at a time. The 5s timer and user actions both call
// refresh(), and overlapping runs would race each other. A refresh requested
// while one is running isn't dropped - it runs once more right after, so
// the screen always ends up showing the latest state.
let refreshing = false;
let refreshAgain = false;
async function refresh() {
  if (refreshing) { refreshAgain = true; return; }
  refreshing = true;
  try {
    do {
      refreshAgain = false;
      await refreshOnce();
    } while (refreshAgain);
  } finally {
    refreshing = false;
  }
}

async function refreshOnce() {
  await flushQueue();  // try to sync anything queued, silently keeps waiting if still offline

  let items, favorites;
  try {
    const [listRes, favRes] = await Promise.all([fetch('/api/list'), fetch('/api/favorites')]);
    items = await listRes.json();
    favorites = await favRes.json();
  } catch (e) {
    updateOfflineBanner(false);
    renderQueueOnly();
    return;
  }
  updateOfflineBanner(true);
  lastItems = items;
  lastFavorites = favorites;

  const favUl = document.getElementById('favList');
  favUl.innerHTML = '';
  favorites.forEach(fav => {
    const li = document.createElement('li');
    li.classList.add('favorite');
    if (fav.needed) li.classList.add('needed');
    li.innerHTML = `
      <input type="checkbox" ${fav.needed ? 'checked' : ''} onchange="toggleFavorite('${fav.id}')">
      <span class="text">${escapeHtml(fav.text)}</span>
      <button class="del" onclick="removeFavorite('${fav.id}')">remove</button>
    `;
    favUl.appendChild(li);
  });

  const ul = document.getElementById('list');
  ul.innerHTML = '';
  items.forEach(item => {
    const li = document.createElement('li');
    if (item.done) li.classList.add('done');
    li.innerHTML = `
      <input type="checkbox" ${item.done ? 'checked' : ''} onchange="toggle('${item.id}')">
      <span class="text">${escapeHtml(item.text)}</span>
      ${item.addedBy ? `<span class="who">${escapeHtml(item.addedBy)}</span>` : ''}
      <button class="del" onclick="removeItem('${item.id}')">x</button>
    `;
    ul.appendChild(li);
  });
  renderQueueRows();
}

// Used when we can't reach the board at all - still show what's queued
// locally so it doesn't look like the item vanished.
function renderQueueOnly() {
  const ul = document.getElementById('list');
  ul.innerHTML = '';
  renderQueueRows();
}

function renderQueueRows() {
  const queue = getQueue();
  const ul = document.getElementById('list');
  queue.forEach(q => {
    const li = document.createElement('li');
    li.classList.add('pending');
    li.innerHTML = `
      <span class="text">${escapeHtml(q.text)}</span>
      <span class="who">queued</span>
      <button class="del" onclick="removeQueued('${q.qid}')">x</button>
    `;
    ul.appendChild(li);
  });
}

// By qid, not list position: a position could point at the wrong item if
// the queue changed between rendering and tapping.
function removeQueued(qid) {
  dequeue(qid);
  refresh();
}

function escapeHtml(s) {
  return s.replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}

document.getElementById('addForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  const text = document.getElementById('itemText').value.trim();
  if (!text) return;
  document.getElementById('itemText').value = '';
  const addedBy = getWho();

  if (!(await postAdd(text, addedBy))) {
    // Board unreachable right now - keep it on this phone instead of
    // losing it. Sent automatically by the next refresh() that gets through.
    enqueue(text, addedBy);
  }
  refresh();
});

async function toggle(id) {
  await fetch('/api/toggle', {method: 'POST', headers: {'Content-Type':'application/json'},
    body: JSON.stringify({id})});
  refresh();
}

async function removeItem(id) {
  await fetch('/api/remove', {method: 'POST', headers: {'Content-Type':'application/json'},
    body: JSON.stringify({id})});
  refresh();
}

async function toggleFavorite(id) {
  await fetch('/api/favorites/toggle', {method: 'POST', headers: {'Content-Type':'application/json'},
    body: JSON.stringify({id})});
  refresh();
}

async function addFavorite() {
  const input = document.getElementById('favText');
  const text = input.value.trim();
  if (!text) return;
  const res = await fetch('/api/favorites/add', {method: 'POST', headers: {'Content-Type':'application/json'},
    body: JSON.stringify({text})});
  if (res.status === 409) {
    alert('Everyday items are full (18). Remove one first.');
    return;
  }
  input.value = '';
  refresh();
}

async function removeFavorite(id) {
  if (!confirm('Remove this from everyday items?')) return;
  await fetch('/api/favorites/remove', {method: 'POST', headers: {'Content-Type':'application/json'},
    body: JSON.stringify({id})});
  refresh();
}

// Two clear buttons, both behind a plain-language confirm - a stray tap
// (or a curious kid) shouldn't be able to wipe the board.
// Items still queued on this phone (added while the board was unreachable)
// are part of the "Also need" list, so both buttons clear them too -
// otherwise they'd reappear on the board at the next sync.

async function clearAlsoNeed() {
  if (!confirm("Remove every 'Also need' item?\n\nEveryday items won't change.")) return;
  try {
    const res = await fetch('/api/clear-all', {method: 'POST'});
    if (!res.ok) throw new Error('board said ' + res.status);
  } catch (e) {
    alert("Couldn't reach the board, so nothing was cleared. Try again on home WiFi.");
    return;
  }
  setQueue([]);
  refresh();
}

async function clearEverything() {
  if (!confirm("Clear the whole board?\n\nThis removes all 'Also need' items " +
               "and unticks all everyday items.")) return;
  clearBoardAfterShopping();  // same action as "Copy list for shopping" -> OK
}

async function resetConfig() {
  if (!confirm('This makes the board forget its WiFi network and restarts it into setup mode ' +
               '(use it after changing router or WiFi password). Your other settings are kept ' +
               'and shown pre-filled. You\'ll need to join the "ShoppingList-Setup" WiFi ' +
               'network on a phone to finish. Continue?')) return;
  try {
    await fetch('/api/reset-config', {method: 'POST'});
  } catch (e) {
    // expected - the board is restarting and dropping this connection
  }
  alert('Board is restarting into setup mode. Connect to the "ShoppingList-Setup" ' +
        'WiFi network on a phone to reconfigure it.');
}

// --- "Copy list for shopping" ------------------------------------------
// Why this is more involved than it looks: this page is served over plain
// http:// by the board, and phones only allow the modern clipboard and
// share APIs on https:// pages - both fail silently here. So:
//  1. Copy with document.execCommand('copy'), the older API that still
//     works on http:// pages.
//  2. It must run DIRECTLY inside the tap (iPhones block copying after any
//     await), so the text is built from the list already on screen
//     (lastItems/lastFavorites, kept fresh by the 5s refresh) rather than
//     fetched again first.
//  3. Copying and clearing are separate steps, and the board is only
//     cleared after a confirmed copy - never on a failed one.
let lastItems = [], lastFavorites = [];

function buildListText() {
  const all = [
    ...lastFavorites.filter(f => f.needed).map(f => f.text),
    ...lastItems.filter(i => !i.done).map(i => i.text),
    ...getQueue().map(q => q.text),
  ];
  if (all.length === 0) return null;
  return 'Shopping list:\n' + all.map(t => '- ' + t).join('\n');
}

// Returns true if the text really reached the clipboard.
function copyText(text) {
  const ta = document.createElement('textarea');
  ta.value = text;
  ta.setAttribute('readonly', '');          // stops the iPhone keyboard popping up
  ta.style.position = 'fixed';
  ta.style.top = '-1000px';
  ta.style.fontSize = '16px';               // stops iPhone zooming the page
  document.body.appendChild(ta);
  ta.select();
  ta.setSelectionRange(0, text.length);     // needed on iPhone
  let ok = false;
  try { ok = document.execCommand('copy'); } catch (e) { ok = false; }
  document.body.removeChild(ta);
  return ok;
}

function showManualCopy(text) {
  const box = document.getElementById('copyBox');
  const ta = document.getElementById('copyText');
  ta.value = text;
  box.style.display = 'block';
}

async function clearBoardAfterShopping() {
  try {
    const res = await fetch('/api/export-complete', { method: 'POST' });
    if (!res.ok) throw new Error('board said ' + res.status);
  } catch (e) {
    alert("Couldn't reach the board, so nothing was cleared. Try again on home WiFi.");
    return;
  }
  setQueue([]);  // queued items were included in the copied list
  refresh();
}

function exportList() {  // deliberately NOT async - see note 2 above
  const text = buildListText();
  if (!text) {
    alert('Nothing to buy right now!');
    return;
  }
  if (!copyText(text)) {
    showManualCopy(text);  // never clears in this case
    return;
  }
  if (confirm('List copied. Paste it into Notes (or a message).\n\n' +
              'Clear the board now? Cancel keeps everything on it.')) {
    clearBoardAfterShopping();
  }
}

// --- Send button settings ------------------------------------------------
// Loaded once when the page opens (not on the 5s refresh, which would
// overwrite whatever you're halfway through typing). The app password is
// never sent back by the board - the box just says whether one is saved.
let passwordSaved = false;
async function loadEmailSettings() {
  try {
    const res = await fetch('/api/settings/email');
    const cfg = await res.json();
    document.getElementById('mailFrom').value = cfg.from || '';
    document.getElementById('mailPass').value = '';
    passwordSaved = !!cfg.passwordSet;
    document.getElementById('mailPass').placeholder = cfg.passwordSet
      ? 'App password saved - leave blank to keep it'
      : 'Gmail app password (16 letters)';
    for (let i = 0; i < 3; i++) {
      const r = (cfg.recipients || [])[i] || {};
      document.getElementById('rName' + i).value = r.name || '';
      document.getElementById('rEmail' + i).value = r.email || '';
      document.getElementById('rPhone' + i).value = r.phone || '';
    }
    renderSendButtons(cfg);
  } catch (e) {
    // Board unreachable - leave the boxes as they are.
  }
}

// --- "Email list to" / "Text list to" buttons ----------------------------
// Email: one button per person with an email address, once email is fully
// set up. The board does the actual send (same as its own Send button);
// this page asks for it, then checks back every second for the result.
// The index sent is the person's position among those WITH an email - the
// same list the board's own Send picker uses.
// Text: one button per person with a mobile number. Nothing goes through the
// board: the button opens this phone's Messages app with the list filled in.
function renderSendButtons(cfg) {
  const box = document.getElementById('emailButtons');
  box.innerHTML = '';
  const all = cfg.recipients || [];
  const withEmail = all.filter(r => r.email);
  if (cfg.from && cfg.passwordSet) {
    withEmail.forEach((r, i) => {
      const b = document.createElement('button');
      b.textContent = 'Email list to ' + r.name;
      b.onclick = () => emailList(i, r.name);
      box.appendChild(b);
    });
  }
  all.filter(r => r.phone).forEach(r => {
    const b = document.createElement('button');
    b.textContent = 'Text list to ' + r.name;
    b.onclick = () => textList(r.phone);
    box.appendChild(b);
  });
}

// Deliberately NOT async: phones only let a page open another app straight
// from a tap, and an await before it would break that. The list is the one
// on screen (refreshed every 5 seconds). "?&body=" is the form that both
// iPhone and Android accept.
function textList(phone) {
  const status = document.getElementById('sendStatus');
  const text = buildListText();
  if (!text) {
    status.textContent = 'Nothing to send - the list is empty.';
    return;
  }
  status.textContent = '';
  window.location.href = 'sms:' + phone + '?&body=' + encodeURIComponent(text);
}

const SEND_MESSAGES = {
  sent:     name => 'Sent to ' + name + '.',
  failed:   name => "Couldn't send to " + name + '. Check the email settings below.',
  empty:    name => 'Nothing to send - the list is empty.',
  nowifi:   name => "The board can't reach the internet right now.",
  cooldown: name => 'Already sent this list to ' + name + ' in the last 2 minutes.',
};

let emailBusy = false;
async function emailList(index, name) {
  if (emailBusy) return;
  const status = document.getElementById('sendStatus');
  emailBusy = true;
  status.textContent = 'Sending to ' + name + '...';
  try {
    const res = await fetch('/api/send', {
      method: 'POST', headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({recipient: index})
    });
    const out = await res.json().catch(() => ({}));
    if (!res.ok) { status.textContent = out.error || 'Not sent.'; return; }
    // The board takes a few seconds (screen refresh + Gmail). Check back
    // every second for up to 30 seconds.
    for (let t = 0; t < 30; t++) {
      await new Promise(r => setTimeout(r, 1000));
      let st;
      try { st = await (await fetch('/api/send-status')).json(); } catch (e) { continue; }
      if (st.id !== out.id) continue;
      const msg = SEND_MESSAGES[st.state];
      if (msg) { status.textContent = msg(st.name); return; }
    }
    status.textContent = "No answer from the board - check its screen to see if it sent.";
  } catch (e) {
    status.textContent = "Couldn't reach the board - not sent. Try again on home WiFi.";
  } finally {
    emailBusy = false;
  }
}

async function saveEmailSettings() {
  const status = document.getElementById('mailStatus');
  const recipients = [];
  for (let i = 0; i < 3; i++) {
    const email = document.getElementById('rEmail' + i).value.trim();
    const phone = document.getElementById('rPhone' + i).value.trim();
    const name = document.getElementById('rName' + i).value.trim();
    if (email || phone) recipients.push({name, email, phone});
  }
  const body = {
    from: document.getElementById('mailFrom').value.trim(),
    password: document.getElementById('mailPass').value,
    recipients,
  };
  status.textContent = 'Saving...';
  try {
    const res = await fetch('/api/settings/email', {
      method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(body)
    });
    const out = await res.json().catch(() => ({}));
    if (!res.ok) { status.textContent = out.error || 'Not saved.'; return; }
  } catch (e) {
    status.textContent = "Couldn't reach the board - not saved. Try again on home WiFi.";
    return;
  }
  const emailReady = body.from && recipients.some(r => r.email) && (body.password || passwordSaved);
  const anyPhone = recipients.some(r => r.phone);
  status.textContent = emailReady
    ? 'Saved. The Send button will appear on the board in a few seconds.'
    : anyPhone
    ? 'Saved. The Text buttons are ready. (Email and the board\'s Send button also need a Gmail address, app password and someone with an email.)'
    : 'Saved. (The Send button needs a Gmail address, app password and someone with an email.)';
  loadEmailSettings();  // show what the board actually stored (e.g. default names)
}

// Firmware version, so people can tell whether their board is up to date.
fetch('/api/version').then(r => r.json())
  .then(v => { document.getElementById('fwVersion').textContent = 'Firmware ' + v.version; })
  .catch(() => {});

ensureWho();
renderWhoLabel();
loadEmailSettings();
refresh();
setInterval(refresh, 5000); // also re-tries the queue if we were offline
</script>
</body>
</html>
)HTML";

const char MANIFEST_JSON[] PROGMEM = R"JSON(
{
  "name": "Shopping List",
  "short_name": "Shopping",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#f4f4f2",
  "theme_color": "#222222"
}
)JSON";
