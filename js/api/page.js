const pageCache = {};
const API_BASE = "https://drums-backend.onrender.com";

export async function fetchPage(pageName) {
  if (pageCache[pageName]) {
    return pageCache[pageName];
  }

  const res = await fetch(`${API_BASE}/api/page/${pageName}`, {
    credentials: "include",
  });

  if (!res.ok) throw new Error(`Failed to fetch page: ${pageName}`);

  const html = await res.text();
  pageCache[pageName] = html;
  return html;
}
