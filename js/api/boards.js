export async function fetchBoards() {
    const res = await fetch('/api/boards');
    if (!res.ok) throw new Error('Failed to fetch boards');
    return await res.json();
}

export async function fetchBoardState(boardId) {
    const res = await fetch(`/api/boards/${boardId}`);
    if (!res.ok) throw new Error('Failed to fetch board state');
    return await res.json();
}

export async function saveBoard(title, state, id = null) {
    const res = await fetch('/api/boards', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: id ? String(id) : undefined, title, state: JSON.stringify(state) })
    });
    if (!res.ok) throw new Error('Failed to save board');
    return await res.json();
}
export async function deleteBoard(id) {
    const res = await fetch(`/api/boards/${id}`, { method: 'DELETE' });
    const data = await res.json();
    if (!res.ok) {
        throw new Error(data.message || 'Failed to delete board');
    }
    return data;
}
