const API_BASE = "https://drums-backend.onrender.com";

export async function startCompile(taskData) {
    const res = await fetch(`${API_BASE}/api/compile`, {
        method: 'POST',
        credentials: 'include',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(taskData)
    });

    if (!res.ok) throw new Error('Failed to start compilation');

    return await res.json();
}

export async function getCompileStatus(taskId) {
    const res = await fetch(`${API_BASE}/api/compile/status?task_id=${taskId}`, {
        credentials: 'include'
    });

    if (!res.ok) throw new Error('Failed to fetch compilation status');

    return await res.json();
}
