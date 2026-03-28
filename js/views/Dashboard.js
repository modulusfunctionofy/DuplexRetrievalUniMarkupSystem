import { root } from '../components/shared.js';
import { checkSession, logout } from '../api/auth.js';
import { navigate } from '../router.js';
import { fetchBoards, fetchBoardState, saveBoard, deleteBoard } from '../api/boards.js';
import { startCompile, getCompileStatus } from '../api/compile.js';

// Server A port on left (blue) (Reference Key), Server B port on right (green) (Response Sheet)
export async function renderDashboard() {
    root.innerHTML = '<div class="auth-wrapper"><div class="auth-card"><p>Verifying session...</p></div></div>';

    let username = "User";
    try {
        const { ok, username: user } = await checkSession();
        if (ok) username = user;
        else {
            navigate('/login', true);
            return;
        }
    } catch (e) {
        navigate('/login', true);
        return;
    }

    // Lock body scrolling for the board, and hide the footer!
    document.body.style.overflow = 'hidden';
    document.body.style.height = '100vh';
    const footer = document.getElementById('footer-container');
    if (footer) footer.style.display = 'none';

    root.innerHTML = `
    <div class="app-shell dashboard-view">
        <button id="sidebar-toggle" class="sidebar-toggle" title="Toggle sidebar">◂</button>
        <div id="sidebar" class="sidebar">
            <div class="sidebar-logo">
                <div class="label">DRUMS</div>
            </div>
            <div style="display:flex; gap:10px; padding:0 20px; margin-top:24px;">
                <button id="new-board-btn" class="new-board-btn" style="flex:1; margin:0; padding:10px 0;">New Board</button>
                <button id="delete-current-board-btn" class="new-board-btn" style="flex:1; margin:0; padding:10px 0; background:#000000; color:#ff4d4f;" title="Delete this board from database">Delete</button>
            </div>
            <div class="history-section">
                <div class="section-label">RECENT BOARDS</div>
                <ul id="history-list" class="history-list"></ul>
            </div>
            <div class="sidebar-footer">
                <div class="user-info">
                    <div class="avatar"></div>
                    <span>${username}</span>
                </div>
                <button id="logout-btn" class="settings-btn" title="Logout" style="color:#e63946; font-size:12px; font-weight:bold;">Log Out</button>
            </div>
        </div>
        <div class="main-area">
            <div class="feature-toolbar">
                <div class="feature-item selected" data-feature="xlsx" title="Download Excel results of semantic evaluation">
                    <div class="feature-circle">X</div>
                    <div class="feature-label">XLSX</div>
                </div>
                <div class="feature-item disabled" data-feature="markup" title="Future Feature: Compare markup structures">
                    <div class="feature-circle">M</div>
                    <div class="feature-label">Markup Comparison</div>
                </div>
                <div class="feature-item disabled" data-feature="mds" title="Future Feature: Multi-dimensional Scaling projection">
                    <div class="feature-circle">M</div>
                    <div class="feature-label">Multi Dimensional Scaling</div>
                </div>
                <div class="feature-item disabled" data-feature="network" title="Future Feature: Visual network of file relations">
                    <div class="feature-circle">N</div>
                    <div class="feature-label">Network Mode</div>
                </div>
                <div class="feature-item disabled" data-feature="rag" title="Future Feature: Retrieval Augmented Generation">
                    <div class="feature-circle">R</div>
                    <div class="feature-label">RAG</div>
                </div>
            </div>
            <div class="upload-toolbar">
                <div class="storage-section">
                    <div class="storage-header">
                        <div class="storage-label"><span class="dot a">&nbsp;&nbsp;&nbsp;&nbsp;●</span> Reference Key</div>
                        <span id="count-a" class="file-count">0 files</span>
                    </div>
                    <div id="drop-a" class="drop-zone" data-side="a">
                        <span class="dz-text">Drop files or click to browse</span>
                        <span class="dz-sub">Blue • Multiple files</span>
                    </div>
                    <input type="file" id="file-input-a" multiple class="hidden" style="display:none">
                    <div id="list-a" class="file-list"></div>
                </div>
                <div class="storage-section">
                    <div class="storage-header">
                        <div class="storage-label"><span class="dot b">&nbsp;&nbsp;&nbsp;&nbsp;●</span> Response Sheet</div>
                        <span id="count-b" class="file-count">0 files</span>
                    </div>
                    <div id="drop-b" class="drop-zone" data-side="b">
                        <span class="dz-text">Drop files or click to browse</span>
                        <span class="dz-sub">Green • Multiple files</span>
                    </div>
                    <input type="file" id="file-input-b" multiple class="hidden" style="display:none">
                    <div id="list-b" class="file-list"></div>
                </div>
            </div>
            <div id="board" class="board">
                <div id="board-canvas" class="board-canvas">
                    <svg id="board-svg" class="board-svg"></svg>
                </div>
            </div>
            <div class="input-bar">
                <form id="chat-form" class="input-wrapper">
                    <textarea id="user-input" rows="1" placeholder="Type a prompt and hit Send... files upload instantly on selection"></textarea>
                    <div class="input-buttons">
                        <button type="submit" class="send-btn" title="Send">→</button>
                        <button type="button" id="compile-btn" class="compile-btn" title="Compile">Compile</button>
                        <button type="button" id="download-btn" class="compile-btn" style="display:none; background:#10b981; margin-left:8px;" title="Download Results">Download .xlsx</button>
                    </div>
                </form>
                <div class="input-hint">PROMPT → BOARD CARD &nbsp;|&nbsp; FILES UPLOAD INSTANTLY</div>
            </div>
        </div>
    </div>
    `;

    document.getElementById('logout-btn').addEventListener('click', async () => {
        await logout();
        cleanupBoard();
        navigate('/', true);
    });

    init(); // from the board logic
}

export function cleanupBoard() {
    document.body.style.overflow = '';
    document.body.style.height = '';
    const footer = document.getElementById('footer-container');
    if (footer) footer.style.display = '';

    // We will dynamically remove the board event listeners
    if (typeof onMouseMove !== 'undefined') {
        document.removeEventListener('mousemove', onMouseMove);
        document.removeEventListener('mouseup', onMouseUp);
    }
}

// ═══════════════════════════════════════════════════════════
//  Canvas AI Board — script.js
//  Features: instant upload, progress animation, timeout,
//  board file-nodes, n8n connections, minimize, sidebar toggle
// ═══════════════════════════════════════════════════════════


// ── State ──────────────────────────────────────────────
const state = {
    currentBoardId: null,
    filesA: [],          // { file, id, status:'uploading'|'done'|'fail', progress:0..100, url }
    filesB: [],
    boardNodes: [],      // { id, side:'a'|'b', name, size, type, url, el, x, y }
    connections: [],     // { id, fromId, toId, pathEl }
    promptCards: [],     // { id, el, x, y, text }
    uploadGroups: [],
    nextId: 1,
    dragging: null,
    connecting: null,
    selectedFeatures: ['xlsx'],
};

let autoSaveTimer = null;
async function triggerAutoSave() {
    if (autoSaveTimer) clearTimeout(autoSaveTimer);
    autoSaveTimer = setTimeout(async () => {
        if (state.boardNodes.length === 0 && state.promptCards.length === 0) return;

        try {
            const boardData = {
                nodes: state.boardNodes.map(n => ({ id: n.id, side: n.side, name: n.name, size: n.size, type: n.type, url: n.url, x: n.x, y: n.y })),
                connections: state.connections.map(c => ({ fromId: c.fromId, toId: c.toId })),
                cards: state.promptCards.map(c => ({ x: c.x, y: c.y, text: c.text }))
            };

            let title = "Untitled Board";
            if (state.promptCards.length > 0) {
                title = state.promptCards[0].text.substring(0, 30);
            } else if (state.boardNodes.length > 0) {
                title = state.boardNodes[0].name.substring(0, 30);
            }

            const res = await saveBoard(title, boardData, state.currentBoardId);
            state.currentBoardId = res.id;
            loadHistory();
        } catch (e) {
            console.error('Auto-save failed:', e);
        }
    }, 1000);
}

const uid = () => 'n' + (state.nextId++);

// ── DOM refs ───────────────────────────────────────────
const $ = (sel) => document.querySelector(sel);
const board = () => $('#board');
const canvas = () => $('#board-canvas');
const svg = () => $('#board-svg');
const sidebar = () => $('#sidebar');
const BOARD_W = 4096, BOARD_H = 4096;

// ── Helpers ────────────────────────────────────────────
function formatSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / 1048576).toFixed(1) + ' MB';
}

function calcTimeout(size) {
    // TLD: size/50KB * 1s, clamped 3–30s
    return Math.max(3000, Math.min(30000, (size / 51200) * 1000));
}

// ── Sidebar toggle ────────────────────────────────────
function initSidebar() {
    const toggle = $('#sidebar-toggle');
    toggle.addEventListener('click', () => {
        const sb = sidebar();
        if (window.innerWidth <= 768) {
            sb.classList.toggle('active');
            sb.classList.remove('collapsed'); // Ensure collapsed doesn't hide it on mobile
        } else {
            sb.classList.toggle('collapsed');
            sb.classList.remove('active');
        }
        const isActive = sb.classList.contains('active');
        const isCollapsed = sb.classList.contains('collapsed');
        toggle.classList.toggle('shifted', isActive || isCollapsed);
        toggle.textContent = (isActive || isCollapsed) ? '\u25B8' : '\u25C2';
    });
}

// ── History ────────────────────────────────────────────
async function loadHistory() {
    try {
        const boards = await fetchBoards();
        const ul = $('#history-list');
        ul.innerHTML = '';
        boards.forEach((b) => {
            const li = document.createElement('li');
            li.className = 'history-item';
            li.textContent = b.title;
            li.onclick = () => openBoard(b.id);
            ul.appendChild(li);
        });
    } catch (e) {
        console.warn('Failed to load history:', e);
    }
}

const openBoard = async (id) => {
    try {
        const data = await fetchBoardState(id);
        clearBoard();
        state.currentBoardId = id;

        // Restore elements
        if (data.nodes) {
            data.nodes.forEach(n => {
                const entry = { id: n.id, name: n.name, size: n.size, type: n.type, url: n.url, status: 'done' };
                spawnFileNode(entry, n.side, n.x, n.y);
            });
        }
        if (data.cards) {
            data.cards.forEach(c => createPromptCard(c.text, c.x, c.y));
        }
        if (data.connections) {
            data.connections.forEach(conn => createConnection(conn.fromId, conn.toId));
        }
    } catch (e) {
        alert('Could not load board.');
    }
}

function clearBoard() {
    state.boardNodes.forEach((n) => n.el && n.el.remove());
    state.promptCards.forEach((c) => c.el && c.el.remove());
    state.uploadGroups.forEach((g) => g.el && g.el.remove());
    state.connections.forEach((c) => c.pathEl && c.pathEl.remove());
    state.boardNodes = [];
    state.promptCards = [];
    state.uploadGroups = [];
    state.connections = [];
    state.filesA = [];
    state.filesB = [];
    state.currentBoardId = null;

    // Update toolbar UI
    renderFileList('a');
    renderFileList('b');
}


// ── New Board ──────────────────────────────────────────
function initNewBoard() {
    $('#new-board-btn').addEventListener('click', () => {
        if (!confirm('Clear the entire board and start new?')) return;
        clearBoard();
    });
    $('#delete-current-board-btn').addEventListener('click', async () => {
        if (!state.currentBoardId) {
            alert("This board hasn't been saved yet.");
            return;
        }
        if (!confirm('Delete this board from your history? Files will remain on the server.')) return;
        try {
            await deleteBoard(state.currentBoardId);
            clearBoard();
            loadHistory();
            alert('Board deleted.');
        } catch (e) {
            alert('Failed to delete board: ' + e.message);
        }
    });
}

// ═══════════════════════════════════════════════════════
//  FILE UPLOAD (instant on select, with progress + TLD)
// ═══════════════════════════════════════════════════════

function setupDropZone(dropId, inputId, side) {
    const zone = document.getElementById(dropId);
    const input = document.getElementById(inputId);
    const activeClass = side === 'a' ? 'active-a' : 'active-b';

    zone.addEventListener('click', () => input.click());

    input.addEventListener('change', (e) => {
        if (e.target.files.length) {
            addFiles(Array.from(e.target.files), side);
            input.value = '';
        }
    });

    zone.addEventListener('dragover', (e) => {
        e.preventDefault();
        zone.classList.add(activeClass);
    });
    zone.addEventListener('dragleave', () => zone.classList.remove(activeClass));
    zone.addEventListener('drop', (e) => {
        e.preventDefault();
        zone.classList.remove(activeClass);
        if (e.dataTransfer.files.length) {
            addFiles(Array.from(e.dataTransfer.files), side);
        }
    });
}

function addFiles(files, side) {
    const arr = side === 'a' ? state.filesA : state.filesB;
    files.forEach((file) => {
        const entry = { file, id: uid(), status: 'uploading', progress: 0 };
        arr.push(entry);
        startRealUpload(entry, side);
    });
    renderFileList(side);
}

async function startRealUpload(entry, side) {
    // Build FormData with file + side
    const formData = new FormData();
    formData.append('side', side);
    formData.append('file', entry.file);

    // Show blinking uploading animation (progress stays at 50% as visual indicator)
    entry.progress = 50;
    renderFileList(side);

    try {
        const res = await fetch('/api/upload', {
            method: 'POST',
            body: formData,
        });

        if (res.ok) {
            const data = await res.json();
            entry.status = 'done';
            entry.progress = 100;
            entry.url = data.url;
            entry.name = entry.file.name;
            entry.size = entry.file.size;
            entry.type = entry.file.type;

            renderFileList(side);
            spawnFileNode(entry, side);
        } else {
            entry.status = 'fail';
            renderFileList(side);
        }
    } catch (err) {
        console.error('Upload failed:', err);
        entry.status = 'fail';
        renderFileList(side);
    }
}

function renderFileList(side) {
    const arr = side === 'a' ? state.filesA : state.filesB;
    const container = document.getElementById(side === 'a' ? 'list-a' : 'list-b');
    const countEl = document.getElementById(side === 'a' ? 'count-a' : 'count-b');

    countEl.textContent = `${arr.length} file${arr.length !== 1 ? 's' : ''}`;
    container.innerHTML = '';

    arr.forEach((entry) => {
        const div = document.createElement('div');
        div.className = 'file-item';

        const statusIcon =
            entry.status === 'done' ? '<span class="fi-status" style="color:#10b981">✓</span>' :
                entry.status === 'fail' ? '<span class="fi-status" style="color:#ef4444">✗</span>' :
                    '<span class="fi-status" style="color:#aaa">⟳</span>';

        const progressClass = side === 'a' ? '' : 'green';
        const failClass = entry.status === 'fail' ? ' fail' : '';
        const uploadingClass = entry.status === 'uploading' ? ' uploading' : '';

        // Show + button to re-add to board if upload is done
        const onBoard = state.boardNodes.some((n) => n.id === entry.id);
        const addBtn = (entry.status === 'done' && !onBoard)
            ? `<button class="fi-add" data-id="${entry.id}" data-side="${side}" title="Add to board">+</button>`
            : '';

        div.innerHTML = `
        ${statusIcon}
        <span class="fi-name" title="${entry.name || entry.file.name}">${entry.name || entry.file.name}</span>
        <span class="fi-size">${formatSize(entry.size || entry.file.size)}</span>
        ${addBtn}
        <button class="fi-remove" data-id="${entry.id}" data-side="${side}" title="Remove">×</button>
        <div class="fi-progress ${progressClass}${failClass}${uploadingClass}" style="width:${entry.progress}%"></div>
      `;
        container.appendChild(div);
    });

    // Remove buttons
    container.querySelectorAll('.fi-remove').forEach((btn) => {
        btn.addEventListener('click', () => {
            const id = btn.dataset.id;
            const s = btn.dataset.side;
            removeFileEntry(id, s);
        });
    });

    // Add-to-board buttons
    container.querySelectorAll('.fi-add').forEach((btn) => {
        btn.addEventListener('click', () => {
            const id = btn.dataset.id;
            const s = btn.dataset.side;
            const arr2 = s === 'a' ? state.filesA : state.filesB;
            const entry = arr2.find((e) => e.id === id);
            if (entry) {
                spawnFileNode(entry, s);
                renderFileList(s);
            }
        });
    });
}

function removeFileEntry(id, side) {
    const arr = side === 'a' ? state.filesA : state.filesB;
    const idx = arr.findIndex((e) => e.id === id);
    if (idx !== -1) {
        arr.splice(idx, 1);
    }
    renderFileList(side);
}

// ═══════════════════════════════════════════════════════
//  BOARD: FILE NODES
// ═══════════════════════════════════════════════════════

let groupCounter = { a: 0, b: 0 };

function spawnFileNode(entry, side, startX = null, startY = null) {
    const c = canvas();
    const b = board();

    // Position: A nodes on left, B nodes on right, stacked vertically
    const existingCount = state.boardNodes.filter((n) => n.side === side).length;
    const baseX = startX !== null ? startX : (side === 'a' ? 40 : Math.min(b.clientWidth, BOARD_W) - 260);
    const baseY = startY !== null ? startY : (40 + existingCount * 60);

    const id = entry.id;
    const el = document.createElement('div');
    el.className = `file-node side-${side}`;
    el.dataset.nodeId = id;
    el.style.left = baseX + 'px';
    el.style.top = baseY + 'px';

    // Icon
    const type = entry.type || entry.file.type;
    const name = entry.name || entry.file.name;
    const size = entry.size || entry.file.size;
    const isImage = type.startsWith('image/');
    let iconHtml;
    if (isImage) {
        const url = entry.url || URL.createObjectURL(entry.file);
        iconHtml = `<div class="fn-icon"><img src="${url}" alt=""></div>`;
    } else {
        const ext = name.split('.').pop().toUpperCase().substring(0, 4);
        iconHtml = `<div class="fn-icon">${ext}</div>`;
    }

    const portClass = side === 'a' ? 'port-a' : 'port-b';

    // Server A port on right, Server B port on left
    const portHtml = `<div class="fn-port ${portClass}" data-node-id="${id}" data-side="${side}" title="Drag to connect"></div>`;

    el.innerHTML = `
      ${side === 'b' ? portHtml : ''}
      ${iconHtml}
      <div class="fn-details">
        <div class="fn-name" title="${name}">${name}</div>
        <div class="fn-meta">${formatSize(size)} • ${side === 'a' ? 'Server A' : 'Server B'}</div>
      </div>
      <div class="fn-actions">
        <button class="minimize-node-btn" title="Minimize">—</button>
        <button class="remove-node-btn" title="Remove">×</button>
      </div>
      ${side === 'a' ? portHtml : ''}
    `;

    c.appendChild(el);

    const nodeObj = { id, side, name, size, type, url: entry.url, el, x: baseX, y: baseY };
    state.boardNodes.push(nodeObj);

    // Drag
    el.addEventListener('mousedown', (e) => {
        if (e.target.classList.contains('fn-port') ||
            e.target.tagName === 'BUTTON') return;
        e.preventDefault();
        startDrag(el, e, 'node');
    });
    el.addEventListener('touchstart', (e) => {
        if (e.target.classList.contains('fn-port') ||
            e.target.tagName === 'BUTTON') return;
        startDrag(el, e, 'node');
    });

    // Minimize
    el.querySelector('.minimize-node-btn').addEventListener('click', () => {
        el.classList.toggle('minimized');
        redrawConnections();
    });

    // Remove
    el.querySelector('.remove-node-btn').addEventListener('click', () => {
        removeNode(id);
        renderFileList(side); // re-render to show + button
    });

    // Port: start connection
    const port = el.querySelector('.fn-port');
    port.addEventListener('mousedown', (e) => {
        e.preventDefault();
        e.stopPropagation();
        startConnection(id, side, e);
    });
    port.addEventListener('touchstart', (e) => {
        e.preventDefault();
        e.stopPropagation();
        startConnection(id, side, e);
    });

    triggerAutoSave();
}

function removeNode(id) {
    const idx = state.boardNodes.findIndex((n) => n.id === id);
    if (idx !== -1) {
        state.boardNodes[idx].el.remove();
        state.boardNodes.splice(idx, 1);
    }
    // Remove associated connections
    state.connections = state.connections.filter((c) => {
        if (c.fromId === id || c.toId === id) {
            c.pathEl.remove();
            return false;
        }
        return true;
    });
    triggerAutoSave();
}

// ═══════════════════════════════════════════════════════
//  BOARD: PROMPT CARDS
// ═══════════════════════════════════════════════════════

function createPromptCard(text, startX = null, startY = null) {
    const c = canvas();
    const b = board();
    const id = uid();

    const el = document.createElement('div');
    el.className = 'board-card';
    el.dataset.cardId = id;

    // Position
    const x = startX !== null ? startX : (b.scrollLeft + b.clientWidth / 2 - 180 + (Math.random() * 60 - 30));
    const y = startY !== null ? startY : (b.scrollTop + b.clientHeight / 3 + (Math.random() * 60 - 30));
    el.style.left = x + 'px';
    el.style.top = y + 'px';

    // Use first few words of prompt as title
    const words = text.split(/\s+/);
    const shortTitle = words.slice(0, 5).join(' ') + (words.length > 5 ? '...' : '');

    el.innerHTML = `
      <div class="card-header">
        <div class="card-title">${escapeHtml(shortTitle)}</div>
        <div class="card-actions">
          <button class="minimize-card-btn" title="Minimize">—</button>
          <button class="close-btn" title="Remove">×</button>
        </div>
      </div>
      <div class="card-body">
        <div class="prompt-text">${escapeHtml(text)}</div>
      </div>
    `;

    c.appendChild(el);
    const cardObj = { id, el, x, y, text };
    state.promptCards.push(cardObj);

    // Drag
    el.addEventListener('mousedown', (e) => {
        if (e.target.tagName === 'BUTTON') return;
        e.preventDefault();
        startDrag(el, e, 'card');
    });
    el.addEventListener('touchstart', (e) => {
        if (e.target.tagName === 'BUTTON') return;
        startDrag(el, e, 'card');
    });

    // Minimize
    el.querySelector('.minimize-card-btn').addEventListener('click', () => {
        el.classList.toggle('minimized');
    });

    // Close
    el.querySelector('.close-btn').addEventListener('click', () => {
        el.remove();
        state.promptCards = state.promptCards.filter((c) => c.id !== id);
        triggerAutoSave();
    });
}

function escapeHtml(str) {
    const d = document.createElement('div');
    d.textContent = str;
    return d.innerHTML;
}

// ═══════════════════════════════════════════════════════
//  DRAG (generic for nodes, cards)
// ═══════════════════════════════════════════════════════

function startDrag(el, e, type) {
    const isTouch = e.type.startsWith('touch');
    const clientX = isTouch ? e.touches[0].clientX : e.clientX;
    const clientY = isTouch ? e.touches[0].clientY : e.clientY;
    const elRect = el.getBoundingClientRect();
    state.dragging = {
        el,
        type,
        offsetX: clientX - elRect.left,
        offsetY: clientY - elRect.top,
    };
    el.style.zIndex = '100';
}

function onMouseMove(e) {
    const isTouch = e.type.startsWith('touch');
    const clientX = isTouch ? e.touches[0].clientX : e.clientX;
    const clientY = isTouch ? e.touches[0].clientY : e.clientY;

    // ── Dragging ──
    if (state.dragging) {
        const b = board();
        const bRect = b.getBoundingClientRect();
        const d = state.dragging;

        // Convert coordinates (account for scroll)
        let newX = clientX - bRect.left + b.scrollLeft - d.offsetX;
        let newY = clientY - bRect.top + b.scrollTop - d.offsetY;

        // Clamp to canvas bounds
        newX = Math.max(0, Math.min(newX, BOARD_W - d.el.offsetWidth));
        newY = Math.max(0, Math.min(newY, BOARD_H - d.el.offsetHeight));

        d.el.style.left = newX + 'px';
        d.el.style.top = newY + 'px';

        if (d.type === 'node') {
            const nodeId = d.el.dataset.nodeId;
            const n = state.boardNodes.find((n) => n.id === nodeId);
            if (n) { n.x = newX; n.y = newY; }
            redrawConnections();
            triggerAutoSave();
        } else if (d.type === 'card') {
            const cardId = d.el.dataset.cardId;
            const c = state.promptCards.find((c) => c.id === cardId);
            if (c) { c.x = newX; c.y = newY; }
            triggerAutoSave();
        }
        if (isTouch) e.preventDefault(); // Prevent scrolling while dragging
    }

    // ── Connecting (temp line) ──
    if (state.connecting) {
        const b = board();
        const bRect = b.getBoundingClientRect();
        const mx = clientX - bRect.left + b.scrollLeft;
        const my = clientY - bRect.top + b.scrollTop;
        updateTempLine(state.connecting.startX, state.connecting.startY, mx, my);
        if (isTouch) e.preventDefault();
    }
}

function onMouseUp(e) {
    if (state.dragging) {
        state.dragging.el.style.zIndex = '60';
        state.dragging = null;
    }

    if (state.connecting) {
        finishConnection(e);
    }
}

// ═══════════════════════════════════════════════════════
//  N8N-STYLE CONNECTIONS (cross-storage only)
// ═══════════════════════════════════════════════════════

function getPortCenter(nodeId) {
    const node = state.boardNodes.find((n) => n.id === nodeId);
    if (!node) return { x: 0, y: 0 };
    const port = node.el.querySelector('.fn-port');
    const b = board();

    // If port is hidden (minimized), compute from node position
    if (!port || port.offsetParent === null) {
        const elRect = node.el.getBoundingClientRect();
        const bRect = b.getBoundingClientRect();
        const px = node.side === 'a'
            ? (elRect.right - bRect.left + b.scrollLeft)
            : (elRect.left - bRect.left + b.scrollLeft);
        const py = elRect.top - bRect.top + b.scrollTop + elRect.height / 2;
        return { x: px, y: py };
    }

    const bRect = b.getBoundingClientRect();
    const pRect = port.getBoundingClientRect();

    return {
        x: pRect.left - bRect.left + b.scrollLeft + pRect.width / 2,
        y: pRect.top - bRect.top + b.scrollTop + pRect.height / 2,
    };
}

function bezierPath(x1, y1, x2, y2) {
    const dx = Math.abs(x2 - x1) * 0.5;
    return `M ${x1} ${y1} C ${x1 + dx} ${y1}, ${x2 - dx} ${y2}, ${x2} ${y2}`;
}

function startConnection(fromId, fromSide, e) {
    const bRect = board().getBoundingClientRect();
    const pos = getPortCenter(fromId);

    // Create temp line
    const s = svg();
    const tempLine = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    tempLine.classList.add('temp-line');
    s.appendChild(tempLine);

    state.connecting = {
        fromId,
        fromSide,
        tempLine,
        startX: pos.x,
        startY: pos.y,
    };
}

function updateTempLine(x1, y1, x2, y2) {
    if (state.connecting && state.connecting.tempLine) {
        state.connecting.tempLine.setAttribute('d', bezierPath(x1, y1, x2, y2));
    }
}

function finishConnection(e) {
    const conn = state.connecting;
    if (!conn) return;

    const isTouch = e.type.startsWith('touch');
    const clientX = isTouch ? e.changedTouches[0].clientX : e.clientX;
    const clientY = isTouch ? e.changedTouches[0].clientY : e.clientY;

    // Remove temp line
    if (conn.tempLine) conn.tempLine.remove();

    // Find if we dropped on a port
    const target = document.elementFromPoint(clientX, clientY);
    if (target && target.classList.contains('fn-port')) {
        const toId = target.dataset.nodeId;
        const toSide = target.dataset.side;

        // Cross-storage only
        if (toSide !== conn.fromSide && toId !== conn.fromId) {
            // Check duplicate
            const exists = state.connections.some(
                (c) =>
                    (c.fromId === conn.fromId && c.toId === toId) ||
                    (c.fromId === toId && c.toId === conn.fromId)
            );

            if (!exists) {
                createConnection(conn.fromId, toId);
                triggerAutoSave();
            }
        }
    }

    state.connecting = null;
}


function createConnection(fromId, toId) {
    const s = svg();
    const pathEl = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    s.appendChild(pathEl);

    const connId = uid();
    const connObj = { id: connId, fromId, toId, pathEl };
    state.connections.push(connObj);

    // Make connection line clickable to delete
    pathEl.style.pointerEvents = 'stroke';
    pathEl.style.cursor = 'pointer';
    pathEl.addEventListener('click', () => {
        if (confirm('Remove this connection?')) {
            pathEl.remove();
            state.connections = state.connections.filter((c) => c.id !== connId);
            triggerAutoSave();
        }
    });

    redrawConnections();
}

function redrawConnections() {
    state.connections.forEach((conn) => {
        const fromPos = getPortCenter(conn.fromId);
        const toPos = getPortCenter(conn.toId);
        conn.pathEl.setAttribute('d', bezierPath(fromPos.x, fromPos.y, toPos.x, toPos.y));
    });
}

// ═══════════════════════════════════════════════════════
//  FORM SUBMIT (prompt → board card)
// ═══════════════════════════════════════════════════════

async function handleCompile() {
    const btn = $('#compile-btn');
    const originalText = btn.textContent;

    // 1. Validation: Ensure all Side A nodes have at least one connection
    const nodesA = state.boardNodes.filter(n => n.side === 'a');
    if (nodesA.length === 0) {
        alert("Please add at least one Reference Key (Side A) file.");
        return;
    }

    const unconnectedA = nodesA.filter(n => {
        return !state.connections.some(c => c.fromId === n.id || c.toId === n.id);
    });

    if (unconnectedA.length > 0) {
        alert(`The following Reference Keys are not connected: ${unconnectedA.map(n => n.name).join(', ')}`);
        return;
    }

    btn.textContent = 'Compiling...';
    btn.disabled = true;

    try {
        // 2. Build Connection Map
        const taskData = {
            timestamp: new Date().toISOString(),
            features: state.selectedFeatures,
            connections: state.connections.map(c => {
                const nodeFrom = state.boardNodes.find(n => n.id === c.fromId);
                const nodeTo = state.boardNodes.find(n => n.id === c.toId);
                // Ensure we know which one is info (A) and which is response (B)
                const ref = nodeFrom.side === 'a' ? nodeFrom : nodeTo;
                const resp = nodeFrom.side === 'b' ? nodeFrom : nodeTo;
                return {
                    ref_name: ref.name,
                    ref_path: ref.url.startsWith('/') ? ref.url.substring(1) : ref.url,
                    resp_name: resp.name,
                    resp_path: resp.url.startsWith('/') ? resp.url.substring(1) : resp.url
                };
            })
        };

        // 3. Send Task to Server via API
        const data = await startCompile(taskData);
        const taskId = data.task_id;

        // 4. Start polling for results
        const downloadBtn = $('#download-btn');
        downloadBtn.style.display = 'none';
        btn.textContent = 'Processing...';

        const poll = async () => {
            try {
                const statusData = await getCompileStatus(taskId);
                if (statusData.status === 'completed') {
                    btn.textContent = originalText;
                    btn.disabled = false;
                    downloadBtn.style.display = 'inline-block';
                    downloadBtn.textContent = 'Download .xlsx';
                    downloadBtn.onclick = () => {
                        const a = document.createElement('a');
                        a.href = statusData.url;
                        a.download = statusData.filename;
                        document.body.appendChild(a);
                        a.click();
                        document.body.removeChild(a);
                    };
                    return; // Stop polling
                }
            } catch (err) {
                console.error('Polling error:', err);
            }
            setTimeout(poll, 3000); // Poll every 3s
        };
        poll();

    } catch (e) {
        console.error(e);
        alert('Failed to start compilation.');
        btn.textContent = originalText;
        btn.disabled = false;
    }
}

function handleSend(e) {
    e.preventDefault();
    const input = $('#user-input');
    const text = input.value.trim();
    if (!text) return;

    createPromptCard(text);
    input.value = '';
    input.style.height = 'auto';
    triggerAutoSave();
}

// ═══════════════════════════════════════════════════════
//  TEXTAREA AUTO-RESIZE
// ═══════════════════════════════════════════════════════

function initTextarea() {
    const ta = $('#user-input');
    ta.addEventListener('input', () => {
        ta.style.height = 'auto';
        ta.style.height = Math.min(ta.scrollHeight, 160) + 'px';
    });
}

// ═══════════════════════════════════════════════════════
//  INIT
// ═══════════════════════════════════════════════════════

function init() {
    initSidebar();
    loadHistory();
    initNewBoard();

    // Setup both upload zones
    setupDropZone('drop-a', 'file-input-a', 'a');
    setupDropZone('drop-b', 'file-input-b', 'b');

    // Initial empty lists
    renderFileList('a');
    renderFileList('b');

    // Canvas Events
    document.addEventListener('mousemove', onMouseMove);
    document.addEventListener('mouseup', onMouseUp);
    document.addEventListener('touchmove', onMouseMove, { passive: false });
    document.addEventListener('touchend', onMouseUp, { passive: false });

    // Form
    $('#chat-form').addEventListener('submit', handleSend);
    $('#compile-btn').addEventListener('click', handleCompile);
    initTextarea();
    initFeatureToolbar();

    // Demo card
    setTimeout(() => {
        createPromptCard('Create a modern landing page for our new AI product');
    }, 500);

    console.log(
        '%c Canvas AI Board ready — instant upload • n8n connections • drag board',
        'color:#3b82f6; font-size:12px'
    );
}

function initFeatureToolbar() {
    const items = document.querySelectorAll('.feature-item');
    items.forEach(item => {
        item.addEventListener('click', () => {
            const feature = item.dataset.feature;
            if (item.classList.contains('disabled')) {
                return;
            }

            // Toggle logic (for when we have more than one selectable)
            if (state.selectedFeatures.includes(feature)) {
                state.selectedFeatures = state.selectedFeatures.filter(f => f !== feature);
                item.classList.remove('selected');
            } else {
                state.selectedFeatures.push(feature);
                item.classList.add('selected');
            }
            triggerAutoSave();
        });
    });
}
