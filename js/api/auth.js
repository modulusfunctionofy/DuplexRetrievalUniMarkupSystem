const API_BASE = "https://drums-backend.onrender.com";

export async function checkSession() {
    try {
        const res = await fetch(`${API_BASE}/api/me`, {
            method: 'GET',
            headers: {
                'Cache-Control': 'no-cache',
                'Pragma': 'no-cache'
            },
            credentials: 'include'
        });

        if (res.ok) {
            const data = await res.json();
            return { ok: true, username: data.username };
        }

        return { ok: false };

    } catch (err) {
        return { ok: false, error: err };
    }
}

export async function login(username, password) {
    const res = await fetch(`${API_BASE}/api/login`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Cache-Control': 'no-cache',
            'Pragma': 'no-cache'
        },
        credentials: 'include',
        body: JSON.stringify({ username, password })
    });

    return res.ok;
}

export async function signup(username, password) {
    const res = await fetch(`${API_BASE}/api/signup`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Cache-Control': 'no-cache',
            'Pragma': 'no-cache'
        },
        credentials: 'include',
        body: JSON.stringify({ username, password })
    });

    return { ok: res.ok, status: res.status };
}

export async function logout() {
    await fetch(`${API_BASE}/api/logout`, {
        method: 'POST',
        credentials: 'include'
    });
}
