import type { RemoteAssetsObs } from 'alt-model/top_level_context/remote_assets_obs';
import type { DefiRecipesObs } from 'alt-model/defi/recipes';
import type { BridgeDataAdaptorObsCache } from './bridge_data_adaptor_cache';
import { createMemo, Obs, Poller } from 'app/util';
import { LazyInitCacheMap } from 'app/util/lazy_init_cache_map';
import { toAdaptorArgs } from '../bridge_adaptor_util';

const POLL_INTERVAL = 5 * 60 * 1000;

export function createAuxDataOptionsObsCache(
  defiRecipesObs: DefiRecipesObs,
  adaptorObsCache: BridgeDataAdaptorObsCache,
  remoteAssetsObs: RemoteAssetsObs,
) {
  return new LazyInitCacheMap((recipeId: string) => {
    // The poller is memoed outside the emitter function so that its last polled time is preserved
    // beyond emitter cleanup.
    const memo = createMemo<Poller>();
    return Obs.combine([defiRecipesObs, adaptorObsCache.get(recipeId), remoteAssetsObs]).mapEmitter<
      bigint[] | undefined
    >((deps, emit) => {
      const [recipes, adaptor, assets] = deps;
      if (!adaptor || !assets || !recipes) return undefined;
      const poller = memo(() => {
        const recipe = recipes.find(x => x.id === recipeId)!;
        const { inA, inB, outA, outB } = toAdaptorArgs(recipe.flow.enter);
        return new Poller(() => {
          adaptor.adaptor.getAuxData(inA, inB, outA, outB).then(emit);
        }, POLL_INTERVAL);
      }, deps);
      return poller.activate();
    }, undefined);
  });
}
