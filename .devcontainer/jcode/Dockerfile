FROM agent-base:local

# Install jcode via official installer (telemetry off for container builds)
# Also install jq for JSON processing in entrypoint.sh
RUN curl -fsSL https://jcode.sh/install | JCODE_NO_TELEMETRY=1 bash && \
  echo "Installed jcode version: $(jcode --version)" && \
  apt-get update && apt-get install -y --no-install-recommends jq && \
  rm -rf /var/lib/apt/lists/*

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
  CMD jcode --version || exit 1

# Preconfigure jcode to talk to the TU Berlin MythLLM LiteLLM proxy.
# Base URLs are placeholders (PLACEHOLDER_CHAT_AI / PLACEHOLDER_SAIA) resolved
# at container startup by entrypoint.sh from the CHAT_ENDPOINT env var.
# API keys are resolved at runtime from CHAT_TOKEN via per-provider env files
# written by entrypoint.sh — never baked into the image.
RUN mkdir -p /root/.config/jcode /root/.jcode
COPY ./config.toml /root/.config/jcode/config.toml
COPY ./config.toml /root/.jcode/config.toml
RUN touch /root/.jcode/no_telemetry

# Entrypoint: constructs jcode env files and base_url from CHAT_TOKEN / CHAT_ENDPOINT
COPY ./entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Display version information
RUN echo "" && \
  echo "\e[34m=================================================================\e[0m" && \
  echo "\e[34m                   JCODE VERSION                                 \e[0m" && \
  echo "\e[34m=================================================================\e[0m" && \
  echo "jcode version: $(jcode --version)" && \
  echo "\e[0m" && \
  echo ""

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/bin/bash"]
