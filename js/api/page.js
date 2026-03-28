const pageCache = {};

export async function fetchPage(pageName) {
    if (pageCache[pageName]) {
        console.log(`[cache HIT] "${pageName}" served from memory`);
        return pageCache[pageName];
    }
    console.log(`[cache MISS] "${pageName}" fetching from server...`);
    const res = await fetch(`/api/page/${pageName}`);
    if (!res.ok) throw new Error(`Failed to fetch page: ${pageName}`);
    const html = await res.text();
    pageCache[pageName] = html;
    return html;
}
