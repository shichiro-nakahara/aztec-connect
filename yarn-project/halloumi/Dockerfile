FROM nakaharashichiro/yarn-project-base AS builder
COPY barretenberg.js barretenberg.js
COPY halloumi halloumi
WORKDIR /usr/src/yarn-project/halloumi
RUN yarn build && yarn test
# Prune dev dependencies. See comment in base image.
RUN yarn cache clean
# enableNetwork set to false in yarn-project-base
RUN yarn config set enableNetwork true
RUN yarn workspaces focus --production > /dev/null
RUN yarn config set enableNetwork false
RUN rm -rf /usr/src/contracts
RUN rm -f .env

FROM node:18-alpine
RUN apk update && apk add --no-cache llvm15-libs
COPY --from=builder /usr/src /usr/src
WORKDIR /usr/src/yarn-project/halloumi
CMD ["yarn", "start"]
EXPOSE 80