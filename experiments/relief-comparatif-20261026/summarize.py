#!/usr/bin/env python3
"""Derived census measures for the French report; no simulation or live access."""
import csv,json,math,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parent
ARMS=('control','unlock','redistribute')

def read(p): return json.loads(p.read_text())
def gini(values):
    v=sorted(values);s=sum(v);n=len(v)
    return 2*sum((i+1)*x for i,x in enumerate(v))/(n*s)-(n+1)/n if s and n else 0

def initial_stats(s):
    ls=s['lineages'];n=sum(l['population'] for l in ls);mean=sum(l['local_cache'] for l in ls)/n
    return {'tick':s['tick'],'cache_total':s['caches']['total'],'cache_occupied':s['caches']['occupied_total'],'cache_occupied_gini':s['caches']['gini_occupied'],'cache_occupied_positive_cells':s['caches']['occupied_positive_cells'],'lineage_cache_percapita_gini_equal_lineage_weights':gini([l['local_cache']/l['population'] for l in ls]),'lineage_cache_percapita_cv_population_weighted':math.sqrt(sum(l['population']*(l['local_cache']/l['population']-mean)**2 for l in ls)/n)/mean,'food':s['resources']['food']}

def main():
    results=ROOT/'results';results.mkdir(exist_ok=True)
    out={'definition':'conditional deterministic paired contrasts; additional window temporally dependent','windows':{}}
    for wi in ('window1','window2'):
        directory=ROOT/'data'/wi;x=read(directory/'analysis.json')
        rows=list(csv.DictReader((directory/'lineage-results.tsv').open(),delimiter='\t'))
        item={'capture':read(directory/'capture.json'),'initial':{},'horizons':{}}
        for arm in ARMS:
            item['initial'][arm]=initial_stats(x['arms'][arm]['initial'])
            t=read(directory/arm/'transformation.json');item['initial'][arm]['transform']=t
            if arm=='redistribute':
                with (directory/arm/'transformation.cells.tsv').open() as f:
                    changes=list(csv.DictReader(f,delimiter='\t'))
                plus=sum(max(0,int(r['cache_after'])-int(r['cache_before'])) for r in changes)
                minus=sum(max(0,int(r['cache_before'])-int(r['cache_after'])) for r in changes)
                assert plus==minus
                item['initial'][arm]['units_reallocated']=plus
        for horizon in (600,4096,8192):
            hh={};ref=x['arms']['control']['horizons'][str(horizon)]
            ref_lineages={int(r['lineage']):r for r in rows if r['arm']=='control' and int(r['horizon'])==horizon}
            for arm in ARMS:
                s=x['arms'][arm]['horizons'][str(horizon)];c=s['initial_cohort'];b=s['counter_deltas_exhaustive']
                arm_lineages=[r for r in rows if r['arm']==arm and int(r['horizon'])==horizon]
                deltas=[]
                for r in arm_lineages:
                    old=ref_lineages[int(r['lineage'])]
                    deltas.append({'lineage':int(r['lineage']),'n0':int(r['initial_population']),'n_final':int(r['population']),'control_final':int(old['population']),'delta_individuals_control':int(r['population'])-int(old['population']),'share_delta_percentage_points':100*(float(r['share'])-float(old['share']))})
                changes=sorted(deltas,key=lambda r:abs(r['delta_individuals_control']),reverse=True)
                hh[arm]={'population':s['population'],'births':b['births'],'deaths':b['deaths'],'births_relative_delta_control':b['births']/ref['counter_deltas_exhaustive']['births']-1,'renewals_N0':s['renewal_births_per_initial_population'],'initial_survivors':c['initial_roots_surviving'],'initial_survival_fraction':c['survival_fraction'],'roots_with_extant_descendants':c['initial_roots_with_extant_descendants'],'fraction_roots_with_extant_descendants':c['with_extant_descendants_fraction'],'roots_with_direct_births':c['initial_roots_with_births'],'lineages':s['lineage_diversity']['count'],'lineages_effective_shannon':s['lineage_diversity']['effective_shannon'],'genotypes_exact':s['genotype_diversity_exact']['count'],'gifts':b['gifts'],'attacks':b['attacks'],'parasites':b['parasites'],'resources':s['resources'],'caches':s['caches'],'population_grid100':s['population_sample_grid_predeclared'],'population_tick_end':s['population_tick_end_secondary_refinement'],'descendant_distribution':s['initial_root_descendant_distribution'],'lineage_share_total_variation_control':sum(abs(r['delta_individuals_control']) for r in changes)/(2*s['population']),'largest_lineage_effects':changes[:8],'extinct_lineages':[int(r['lineage']) for r in arm_lineages if int(r['population'])==0]}
            item['horizons'][str(horizon)]=hh
        out['windows'][wi]=item
        for name in ('analysis.json','lineage-results.tsv','comparison.tsv','capture.json'):
            shutil.copyfile(directory/name,results/f'{wi}-{name}')
    (results/'summary.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
    for wi,w in out['windows'].items():
        for arm,s in w['horizons']['8192'].items():
            print(wi,arm,'births',s['births'],'roots-desc',s['roots_with_extant_descendants'],'lineages',s['lineages'],'genomes',s['genotypes_exact'],'TV',round(s['lineage_share_total_variation_control'],5))
if __name__=='__main__':main()
