import 'log-timestamp';
import 'source-map-support/register';
import { run } from './run';

const {
  ETHEREUM_HOST = 'http://localhost:8545',
  PRIVATE_KEY = '',
  AGENT_TYPE = 'payment',
  ASSETS = '0',
  NUM_AGENTS = '1',
  NUM_TXS_PER_AGENT = '1',
  NUM_CONCURRENT_TXS = '1',
  ROLLUP_HOST = 'http://localhost:8081',
  CONFS = '1',
  LOOPS,
} = process.env;

async function main() {
  const shutdown = () => process.exit(0);
  process.once('SIGINT', shutdown);
  process.once('SIGTERM', shutdown);

  await run(
    Buffer.from(PRIVATE_KEY, 'hex'),
    AGENT_TYPE,
    +NUM_AGENTS,
    +NUM_TXS_PER_AGENT,
    +NUM_CONCURRENT_TXS,
    ASSETS.split(',').map(x => +x),
    ROLLUP_HOST,
    ETHEREUM_HOST,
    +CONFS,
    LOOPS ? +LOOPS : undefined,
  );
}

main().catch(console.log);
