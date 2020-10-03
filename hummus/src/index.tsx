import './styles/guacamole.css';
import { Block, FlexBox, PageSteps, SwitchInput, Text, TextButton } from '@aztec/guacamole-ui';
import { WebSdk } from 'aztec2-sdk';
import debug from 'debug';
import React, { useState } from 'react';
import ReactDOM from 'react-dom';
import { BrowserRouter, Link, Route, RouteComponentProps, Switch, useLocation } from 'react-router-dom';
import styled, { createGlobalStyle } from 'styled-components';
import { ActionForm } from './action_form';
import { ThemeContext, themes } from './config/context';
import { GlobalState, LocalState, RollupDetails, TxDetails } from './data_explorer';
import { Init } from './init';
import { Terminal, TerminalComponent } from './terminal';
require('barretenberg/wasm/barretenberg.wasm');

declare global {
  interface Window {
    web3: any;
    ethereum: any;
  }
}

const GlobalStyle = createGlobalStyle`
  #root {
    height: 100vh;
    overflow: hidden;
  }
`;

const Container = ({
  className,
  background,
  children,
}: {
  className?: string;
  background: string;
  children: React.ReactNode;
}) => (
  <Block className={className} padding="xl" align="center" background={background} stretch>
    {children}
  </Block>
);

const StyledContainer = styled(Container)`
  height: 100vh;
  overflow: auto;
`;

const StyledContent = styled.div`
  width: 100%;
  max-width: 640px;
`;

const tabs = [
  { title: 'Send', href: '/' },
  { title: 'Transactions', href: '/transactions' },
  { title: 'Explorer', href: '/explorer' },
];

interface RollupRouteParams {
  id: string;
}

type RollupRouteProps = RouteComponentProps<RollupRouteParams>;

interface TxRouteParams {
  txHash: string;
}

type TxRouteProps = RouteComponentProps<TxRouteParams>;

const Unsupported = () => {
  const [theme] = useState(themes.dark);
  return (
    <ThemeContext.Provider value={theme}>
      <StyledContainer background={theme.background}>
        <FlexBox align="center">
          <StyledContent>
            <Block padding="m 0 xl">This application requires Chrome with the MetaMask extension installed.</Block>
          </StyledContent>
        </FlexBox>
      </StyledContainer>
    </ThemeContext.Provider>
  );
};

function ThemedContent({ app }: { app: WebSdk }) {
  const [theme, setTheme] = useState(themes[window.localStorage.getItem('theme') === 'light' ? 'light' : 'dark']);
  const { pathname } = useLocation();
  const serverUrl = window.location.origin + '/falafel';

  return (
    <ThemeContext.Provider value={theme}>
      <StyledContainer background={theme.background}>
        <FlexBox align="center">
          <StyledContent>
            <Block padding="m 0 xl">
              <PageSteps
                theme={theme.theme === 'light' ? 'primary' : 'white'}
                steps={tabs.map(({ title, href }) => ({ title, href, Link }))}
                currentStep={tabs.findIndex(({ href }) => href === pathname) + 1}
                withoutIndex
              />
            </Block>
            <Init initialServerUrl={serverUrl} app={app}>
              {({ account }) => (
                <Switch>
                  <Route
                    path="/rollup/:id"
                    component={({ match }: RollupRouteProps) => <RollupDetails app={app} id={+match.params.id} />}
                  />
                  <Route
                    path="/tx/:txHash"
                    component={({ match }: TxRouteProps) => (
                      <TxDetails app={app} txHash={Buffer.from(match.params.txHash, 'hex')} />
                    )}
                  />
                  <Route exact path="/transactions">
                    <LocalState app={app} />
                  </Route>
                  <Route exact path="/explorer">
                    <GlobalState app={app} />
                  </Route>
                  <Route>
                    <ActionForm app={app} account={account} />
                  </Route>
                </Switch>
              )}
            </Init>
            <Block padding="xl 0">
              <FlexBox valign="center" align="space-between">
                <FlexBox valign="center">
                  <SwitchInput
                    theme={theme.theme}
                    onClick={() => {
                      const nextTheme = theme.theme === 'light' ? 'dark' : 'light';
                      window.localStorage.setItem('theme', nextTheme);
                      setTheme(themes[nextTheme]);
                    }}
                    checked={theme.theme === 'light'}
                  />
                  <Block left="m">
                    <Text text={theme.theme === 'light' ? 'Light Mode' : 'Dark Mode'} />
                  </Block>
                </FlexBox>
              </FlexBox>
            </Block>
          </StyledContent>
        </FlexBox>
      </StyledContainer>
    </ThemeContext.Provider>
  );
}

function LandingPage({ app }: { app: WebSdk }) {
  return (
    <Switch>
      <Route path="/form" component={() => <ThemedContent app={app} />} />
      <Route>
        <TerminalComponent app={app} terminal={new Terminal(12, 40)} />
      </Route>
    </Switch>
  );
}

async function main() {
  if (!debug.enabled('bb:')) {
    debug.enable('bb:*');
    location.reload();
  }
  if (!window.ethereum) {
    ReactDOM.render(
      <BrowserRouter>
        <GlobalStyle />
        <Unsupported />
      </BrowserRouter>,
      document.getElementById('root'),
    );
  } else {
    // Have to do this early to silence warning.
    window.ethereum.autoRefreshOnNetworkChange = false;

    const app = new WebSdk(window.ethereum);
    ReactDOM.render(
      <BrowserRouter>
        <GlobalStyle />
        <LandingPage app={app} />
      </BrowserRouter>,
      document.getElementById('root'),
    );
  }
}

// tslint:disable-next-line:no-console
main().catch(console.error);
