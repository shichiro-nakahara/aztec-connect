import React from 'react';
import ReactDOM from 'react-dom';
import { BrowserRouter } from 'react-router-dom';
import 'ui-components/styles/reset.css';
import 'ui-components/styles/global.css';
import { getConfig } from './config';
import { Views } from './views';
import { TopLevelContextProvider } from 'alt-model/top_level_context/top_level_context_provider';

declare global {
  interface Window {
    ethereum: any;
  }
}

async function main() {
  const config = await getConfig();

  ReactDOM.render(
    <TopLevelContextProvider config={config}>
      <BrowserRouter>
        <Views config={config} />
      </BrowserRouter>
    </TopLevelContextProvider>,
    document.getElementById('root'),
  );
}

main().catch(console.log);
