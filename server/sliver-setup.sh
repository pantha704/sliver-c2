#!/bin/bash
# Sliver C2 Setup for NEW machine
# Installs Sliver + Cloudflared + Wrangler on any fresh Ubuntu/Debian
# Usage: bash sliver-setup.sh
# After running, use: bash c2-start.sh

set -e
CF_API_TOKEN="${CF_API_TOKEN:?Set CF_API_TOKEN env var (from your Cloudflare dashboard)}"
NODE_VERSION="lts"

echo "[*] Installing dependencies..."
sudo apt-get update -qq
sudo apt-get install -y -qq curl wget git python3-minimal openssh-client 2>/dev/null

# Install Node.js via mise/npm if wrangler needed
if ! command -v npx &>/dev/null; then
    echo "[*] Installing Node.js..."
    curl -fsSL https://deb.nodesource.com/setup_22.x | sudo -E bash - 2>/dev/null
    sudo apt-get install -y -qq nodejs 2>/dev/null
fi

# Install cloudflared
if ! command -v cloudflared &>/dev/null; then
    echo "[*] Installing cloudflared..."
    curl -sL https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64 \
        -o /tmp/cloudflared && sudo mv /tmp/cloudflared /usr/local/bin/cloudflared && sudo chmod +x /usr/local/bin/cloudflared
fi

# Install Sliver
if ! command -v sliver &>/dev/null; then
    echo "[*] Installing Sliver C2..."
    curl -sL https://sliver.sh/install | sudo bash 2>/dev/null || {
        echo "[*] Manual Sliver install..."
        wget -q https://github.com/BishopFox/sliver/releases/download/v1.7.3/sliver-server_linux-amd64 \
            -O /tmp/sliver && sudo mv /tmp/sliver /usr/local/bin/sliver && sudo chmod +x /usr/local/bin/sliver
    }
fi

echo ""
echo "[+] Setup complete."
echo "    Run:  bash c2-start.sh"
echo "    Console: sliver"
echo ""
echo "    Ensure CF_API_TOKEN is set in your environment."
echo "    Copy target/ folder to your Windows machine."
