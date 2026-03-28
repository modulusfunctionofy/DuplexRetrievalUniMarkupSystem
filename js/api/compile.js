export async function startCompile(taskData) {
    const res = await fetch('/api/compile', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(taskData)
    });
    if (!res.ok) throw new Error('Failed to start compilation');
    return await res.json();
}

export async function getCompileStatus(taskId) {
    const res = await fetch(`/api/compile/status?task_id=${taskId}`);
    if (!res.ok) throw new Error('Failed to fetch compilation status');
    return await res.json();
}
