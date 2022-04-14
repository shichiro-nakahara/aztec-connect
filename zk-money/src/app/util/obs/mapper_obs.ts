import { BaseObs } from './base_obs';
import { IObs, ObsUnlisten } from './types';

export class MapperObs<TIn, TOut> extends BaseObs<TOut> {
  private unlistenDep?: ObsUnlisten;
  private neverMapped = true;
  private lastMappedDepValue?: TIn;
  constructor(private readonly dep: IObs<TIn>, private readonly mapper: (value: TIn) => TOut) {
    super(mapper(dep.value));
  }
  private refresh = (value: TIn) => {
    this.neverMapped = false;
    this.lastMappedDepValue = value;
    this.setAndEmit(this.mapper(value));
  };
  protected didReceiveFirstListener() {
    if (this.neverMapped || this.lastMappedDepValue !== this.dep.value) {
      this.refresh(this.dep.value);
    }
    this.unlistenDep = this.dep.listen(this.refresh);
  }
  protected didLoseLastListener() {
    this.unlistenDep?.();
    this.unlistenDep = undefined;
  }
}
