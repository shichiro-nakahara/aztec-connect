import { DefiRecipe } from '../../../alt-model/defi/types';
import { CardAssetTag, CardTag } from '../../card_tags';
import style from './defi_card_header.module.scss';

export const DefiCardHeader = ({ recipe }: { recipe: DefiRecipe }) => {
  const { cardTag, logo, flow } = recipe;
  return (
    <div className={style.cardHeader}>
      <img className={style.cardHeaderLogo} src={logo} alt="logo" />
      <div className={style.cardHeaderButtonsWrapper}>
        <CardTag>{cardTag}</CardTag>
        <CardAssetTag asset={flow.enter.inA} />
      </div>
    </div>
  );
};
