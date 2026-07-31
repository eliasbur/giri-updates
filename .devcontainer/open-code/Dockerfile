FROM agent-base:local

# Install Open Code
RUN npm install -g opencode-ai && \
  echo "Installed Open Code version: $(opencode --version)"

# Preconfigure opencode to talk to the TU Berlin MythLLM LiteLLM proxy.
# The API key is resolved at runtime from CHAT_TOKEN (see {env:CHAT_TOKEN} in opencode.json),
# so it must be passed in via --env-file .env / -e CHAT_TOKEN, never baked into the image.
RUN mkdir -p /root/.config/opencode
COPY ./opencode.json /root/.config/opencode/opencode.json

# Display version information
RUN echo "" && \
  echo "\e[34m=================================================================\e[0m" && \
  echo "\e[34m                   OPEN CODE VERSION                             \e[0m" && \
  echo "\e[34m=================================================================\e[0m" && \
  echo "\e[34m" && opencode --version && \
  echo "\e[0m" && \
  echo ""

CMD ["/bin/bash"]
