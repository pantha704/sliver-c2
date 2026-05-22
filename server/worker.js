export default {
  async fetch(request, env) {
    const url = new URL(request.url)
    const target = 'https://competition-environment-streaming-shelter.trycloudflare.com' + url.pathname + url.search
    const body = request.method !== 'GET' && request.method !== 'HEAD' ? await request.arrayBuffer() : undefined
    const modified = new Request(target, {
      method: request.method,
      headers: request.headers,
      body
    })
    try { return await fetch(modified) } catch(e) { return new Response('offline', { status: 503 }) }
  }
}