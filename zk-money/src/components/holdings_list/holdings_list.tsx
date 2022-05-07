import type { DefiRecipe } from 'alt-model/defi/types';
import { useState } from 'react';
import { Toggle } from 'ui-components';
import { TokenList } from './token_list';
import style from './holdings_list.module.scss';
import { DefiInvestments } from 'components/defi_investments';

const VIEWS = [
  { label: 'Tokens', value: 'tokens' },
  { label: 'Earn Positions', value: 'defi-positions' },
];

type View = typeof VIEWS[number]['value'];

interface HoldingsListProps {
  onOpenDefiExitModal: (recipe: DefiRecipe) => void;
}

export function HoldingsList(props: HoldingsListProps) {
  const [view, setView] = useState<View>('tokens');

  return (
    <div className={style.holdingsListWrapper}>
      <div className={style.speedSwitchWrapper}>
        <Toggle options={VIEWS} value={view} onChangeValue={setView} />
      </div>
      <>{view === 'tokens' ? <TokenList /> : <DefiInvestments onOpenDefiExitModal={props.onOpenDefiExitModal} />}</>
    </div>
  );
}
