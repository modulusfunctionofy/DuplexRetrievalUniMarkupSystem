const API_BASE = "https://drums-backend.onrender.com";

export async function fetchBoards() {
    const res = await fetch(`${API_BASE}/api/boards`, {
        credentials: 'include'
    });

    if (!res.ok) throw new Error('Failed to fetch boards');

    return await res.json();
}

export async function fetchBoardState(boardId) {
    const res = await fetch(`${API_BASE}/api/boards/${boardId}`, {
        credentials: 'include'
    });

    if (!res.ok) throw new Error('Failed to fetch board state');

    return await res.json();
}

export async function saveBoard(title, state, id = null) {
    const res = await fetch(`${API_BASE}/api/boards`, {
        method: 'POST',
        credentials: 'include',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            id: id ? String(id) : undefined,
            title,
            state: JSON.stringify(state)
        })
    });

    if (!res.ok) throw new Error('Failed to save board');

    return await res.json();
}

export async function deleteBoard(id) {
    const res = await fetch(`${API_BASE}/api/boards/${id}`, {
        method: 'DELETE',
        credentials: 'include'
    });

    const data = await res.json();

    if (!res.ok) {
        throw new Error(data.message || 'Failed to delete board');
    }

    return data;
}
