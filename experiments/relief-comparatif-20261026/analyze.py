#!/usr/bin/env python3
"""Analyse prospective exhaustive des cohortes Relief sur copies seulement.

Usage: python3 analyze.py data/window1 [--horizons 600,4096,8192]
Read-only inputs per arm (control, unlock, redistribute): initial.json,
initial.agents.tsv, atH.json, atH.agents.tsv, events.tsv, series.tsv. Outputs, written
atomically in WINDOW_DIR: analysis.json, lineage-results.tsv,
cohort-results.tsv (last horizon only), comparison.tsv.

Each initial living ID starts a distinct prospective cohort. A child inherits
its parent's initial cohort, even when that root is already dead at the final
horizon. Pre-existing ancestry among initial IDs is deliberately not merged.
Birth/death rows are exhaustive. Other event rows are a deterministic sample;
no sampled event count is treated as a global interaction count or confidence
interval. All assertions are mandatory and cause failure rather than partial
results when genealogy or checkpoint totals disagree.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import csv
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import tempfile

ARMS = ("control", "unlock", "redistribute")
COUNTERS = ("births", "deaths", "moves", "eats", "builds", "breaks", "gifts",
            "attacks", "parasites", "mutations", "energy_spent", "solar_food",
            "sensor_witness", "memory_witness", "witness_samples")
REQUIRED_AGENT_FIELDS = {"id", "parent", "lineage", "born", "age", "energy", "body", "store", "genotype_hash", "genome_hex"}
REQUIRED_EVENT_FIELDS = {"tick", "kind", "id", "lineage", "other", "position", "amount", "genotype_hash", "genome_hex"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def ratio(numerator, denominator):
    return numerator / denominator if denominator else None


def diversity(counts):
    """Natural-log Shannon and Hill numbers; exact integer genotype keys."""
    values = [n for n in counts.values() if n]
    total = sum(values)
    if not total:
        return {"count": 0, "entropy_nats": 0.0, "effective_shannon": 0.0,
                "effective_simpson": 0.0, "simpson_heterozygosity": 0.0}
    squares = sum(n * n for n in values)
    entropy = math.log(total) - sum(n * math.log(n) for n in values) / total
    return {"count": len(values), "entropy_nats": entropy,
            "effective_shannon": math.exp(entropy),
            "effective_simpson": total * total / squares,
            "simpson_heterozygosity": 1 - squares / (total * total)}


def descendant_distribution(values):
    """Census distribution across all initial roots, including zeros."""
    ordered = sorted(values)
    count, total = len(ordered), sum(ordered)
    def quantile(q):
        return ordered[max(0, math.ceil(q * count) - 1)] if count else None
    gini = (2 * sum((i + 1) * value for i, value in enumerate(ordered)) / (count * total) - (count + 1) / count) if count and total else None
    top_count = math.ceil(count / 100)
    return {"initial_roots_denominator": count, "living_descendants_total": total,
            "roots_with_extant_descendants": sum(value > 0 for value in ordered),
            "mean": ratio(total, count), "median_nearest_rank": quantile(0.5),
            "p90_nearest_rank": quantile(0.9), "p99_nearest_rank": quantile(0.99),
            "max": max(ordered) if ordered else None, "gini_including_zero_roots": gini,
            "top1pct_initial_roots_count": top_count,
            "top1pct_initial_roots_descendant_share": ratio(sum(ordered[-top_count:]) if top_count else 0, total)}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_state(prefix):
    path = prefix.with_suffix(".json")
    with path.open() as stream:
        state = json.load(stream)
    for key in ("tick", "population", "rng_hex", "initial_matter", "matter", "config", "counters", "resources", "diversity", "caches", "lineages"):
        require(key in state, f"{path}: missing {key}")
    require(state["matter"] == state["initial_matter"], f"{path}: matter not conserved")
    require(sum(state["resources"][key] for key in ("mineral", "food", "detritus", "structure", "cache", "body", "store")) == state["matter"], f"{path}: resource sum mismatch")
    require(state["population"] <= state["config"]["capacity"], f"{path}: over capacity")
    for name in COUNTERS:
        require(name in state["counters"], f"{path}: missing counter {name}")
    agents = {}
    with prefix.with_suffix(".agents.tsv").open(newline="") as stream:
        reader = csv.DictReader(stream, delimiter="\t")
        require(REQUIRED_AGENT_FIELDS <= set(reader.fieldnames or []), f"{prefix}: wrong agents header")
        for row in reader:
            for key in REQUIRED_AGENT_FIELDS - {"genotype_hash", "genome_hex"}:
                row[key] = int(row[key])
            ident = row["id"]
            require(ident not in agents, f"{prefix}: duplicate living ID {ident}")
            genome = row["genome_hex"].lower()
            require(len(genome) == 128 and len(bytes.fromhex(genome)) == 64, f"{prefix}: invalid genome {ident}")
            row["genome_hex"] = genome
            agents[ident] = row
    require(len(agents) == state["population"], f"{prefix}: population export mismatch")
    lineages = Counter(row["lineage"] for row in agents.values())
    require(dict(lineages) == {row["lineage"]: row["population"] for row in state["lineages"]}, f"{prefix}: lineage export mismatch")
    exact = len({row["genome_hex"] for row in agents.values()})
    require(exact == state["diversity"]["genotypes_exact"], f"{prefix}: exact genome export mismatch")
    return state, agents


@dataclass(slots=True)
class Node:
    lineage: int
    root: int
    depth: int
    parent: int
    born: int
    genome: str


class ArmAnalysis:
    def __init__(self, directory, horizons):
        self.directory = directory
        self.horizons = horizons
        self.initial, self.roots = read_state(directory / "initial")
        self.initial_lineages = Counter(row["lineage"] for row in self.roots.values())
        self.alive = {ident: Node(row["lineage"], ident, 0, row["parent"], row["born"], row["genome_hex"]) for ident, row in self.roots.items()}
        self.seen_ids = set(self.roots)
        self.last_new_id = max(self.roots, default=0)
        self.births = Counter()
        self.deaths = Counter()
        self.initial_deaths = Counter()
        self.newborn_deaths = Counter()
        self.root_births = Counter()
        self.root_direct_births = Counter()
        self.root_descendant_deaths = Counter()
        self.root_max_depth = Counter()
        self.root_death_tick = {}
        self.samples = defaultdict(Counter)
        self.last_event_tick = self.initial["tick"]
        self.census_through_tick = self.initial["tick"]
        self.population_tick_histogram = Counter()
        self.ignored_preinitial_rows = 0
        self.snapshots = {}
        self.lineage_rows = []
        self.final_cohort_rows = []
        self.hashes = {}
        self.extinction_state = None
        self.series_rows = {}
        with (directory / "series.tsv").open(newline="") as stream:
            reader = csv.DictReader(stream, delimiter="\t")
            require({"tick", "population", "births", "deaths"} <= set(reader.fieldnames or []), f"{directory}: wrong series header")
            for row in reader:
                values = {key: int(row[key]) for key in ("tick", "population", "births", "deaths")}
                tick = values["tick"]
                if tick < self.initial["tick"]:
                    continue
                require(tick <= self.initial["tick"] + horizons[-1], f"{directory}: series exceeds last horizon")
                if tick in self.series_rows:
                    require(values == self.series_rows[tick], f"{directory}: conflicting duplicate series tick {tick}")
                self.series_rows[tick] = values
        if self.initial["tick"] in self.series_rows:
            row = self.series_rows[self.initial["tick"]]
            require(row["population"] == self.initial["population"] and all(row[key] == self.initial["counters"][key] for key in ("births", "deaths")), f"{directory}: initial series differs from export")
        self.series_unchecked = sorted(tick for tick in self.series_rows if tick > self.initial["tick"])
        self.series_cursor = 0

    def record_population_through(self, tick):
        # Event order is exact; once all events of a tick were consumed, the
        # living-ID count is that tick's final census. Empty gaps retain it.
        duration = tick - self.census_through_tick
        require(duration >= 0, f"{self.directory}: population census moved backwards")
        if duration:
            while self.series_cursor < len(self.series_unchecked) and self.series_unchecked[self.series_cursor] <= tick:
                row = self.series_rows[self.series_unchecked[self.series_cursor]]
                require(row["population"] == len(self.alive), f"{self.directory}: series population differs from event census at {row['tick']}")
                require(row["births"] == self.initial["counters"]["births"] + sum(self.births.values()) and row["deaths"] == self.initial["counters"]["deaths"] + sum(self.deaths.values()), f"{self.directory}: series birth/death counters differ from event census at {row['tick']}")
                self.series_cursor += 1
            self.population_tick_histogram[len(self.alive)] += duration
            self.census_through_tick = tick

    def event(self, row):
        tick = int(row["tick"])
        require(tick >= self.last_event_tick, f"{self.directory}: events out of order at {tick}")
        if tick > self.last_event_tick:
            self.record_population_through(tick - 1)
        self.last_event_tick = tick
        kind = row["kind"]
        ident, lineage, other = (int(row[key]) for key in ("id", "lineage", "other"))
        require(kind != "founder", f"{self.directory}: forbidden founder event at {tick}")
        if kind == "birth":
            require(ident not in self.seen_ids and ident > self.last_new_id, f"{self.directory}: reused/nonmonotonic newborn ID {ident}")
            require(other in self.alive, f"{self.directory}: parent {other} not alive for child {ident}")
            parent = self.alive[other]
            require(parent.lineage == lineage, f"{self.directory}: child lineage differs from parent")
            genome = row["genome_hex"].lower()
            require(len(genome) == 128 and len(bytes.fromhex(genome)) == 64, f"{self.directory}: birth without full genome")
            depth = parent.depth + 1
            self.alive[ident] = Node(lineage, parent.root, depth, other, tick, genome)
            self.seen_ids.add(ident)
            self.last_new_id = ident
            self.births[lineage] += 1
            self.root_births[parent.root] += 1
            self.root_direct_births[parent.root] += other == parent.root
            self.root_max_depth[parent.root] = max(self.root_max_depth[parent.root], depth)
        elif kind == "death":
            require(ident in self.alive, f"{self.directory}: death of absent ID {ident}")
            node = self.alive.pop(ident)
            require(node.lineage == lineage, f"{self.directory}: death lineage mismatch")
            self.deaths[lineage] += 1
            if ident in self.roots:
                self.initial_deaths[lineage] += 1
                self.root_death_tick[ident] = tick
            else:
                self.newborn_deaths[lineage] += 1
                self.root_descendant_deaths[node.root] += 1
        else:
            require(kind in ("build", "deposit", "gift", "attack", "parasite"), f"{self.directory}: unknown event kind {kind}")
            require(ident in self.alive, f"{self.directory}: sampled actor {ident} not alive")
            require(self.alive[ident].lineage == lineage, f"{self.directory}: sampled actor lineage mismatch")
            sample = self.samples[kind]
            sample["rows"] += 1
            sample["amount"] += int(row["amount"])
            if kind in ("gift", "attack", "parasite"):
                require(other in self.alive, f"{self.directory}: sampled recipient {other} not alive")
                sample["recipient_resolved"] += 1
                sample["same_lineage"] += self.alive[other].lineage == lineage

    def snapshot(self, horizon):
        prefix = self.directory / f"at{horizon}"
        self.record_population_through(self.initial["tick"] + horizon)
        require(sum(self.population_tick_histogram.values()) == horizon, f"{prefix}: census duration mismatch")
        state, agents = read_state(prefix)
        target_tick = self.initial["tick"] + horizon
        require(state["tick"] == target_tick or (state["population"] == 0 and self.initial["tick"] <= state["tick"] < target_tick), f"{prefix}: wrong horizon tick for non-extinct state")
        require(self.last_event_tick <= state["tick"], f"{prefix}: events occur after effective checkpoint tick")
        if self.extinction_state is not None:
            require(state == self.extinction_state, f"{prefix}: extinct checkpoint changed or was reseeded")
        if state["population"] == 0:
            self.extinction_state = state
        require(state["initial_matter"] == self.initial["initial_matter"], f"{prefix}: initial matter changed")
        require(state["config"] == self.initial["config"], f"{prefix}: config changed")
        require(set(agents) == set(self.alive), f"{prefix}: genealogy/alive IDs differ from checkpoint")
        for ident, row in agents.items():
            node = self.alive[ident]
            require(row["lineage"] == node.lineage and row["parent"] == node.parent and row["born"] == node.born and row["genome_hex"] == node.genome,
                    f"{prefix}: genealogy/genome mismatch for {ident}")
        counters = {name: state["counters"][name] - self.initial["counters"][name] for name in COUNTERS}
        require(all(value >= 0 for value in counters.values()), f"{prefix}: decreasing cumulative counter")
        require(counters["births"] == sum(self.births.values()), f"{prefix}: birth events/counter mismatch")
        require(counters["deaths"] == sum(self.deaths.values()), f"{prefix}: death events/counter mismatch")
        require(state["population"] == self.initial["population"] + counters["births"] - counters["deaths"], f"{prefix}: population flow mismatch")
        pop_by_lineage = Counter(row["lineage"] for row in agents.values())
        genomes = Counter(row["genome_hex"] for row in agents.values())
        lineage_genomes = defaultdict(Counter)
        for row in agents.values():
            lineage_genomes[row["lineage"]][row["genome_hex"]] += 1
        alive_descendants = Counter()
        alive_descendant_depth = Counter()
        surviving_initial = Counter()
        for ident, node in self.alive.items():
            if node.depth:
                alive_descendants[node.root] += 1
                alive_descendant_depth[node.root] = max(alive_descendant_depth[node.root], node.depth)
            else:
                surviving_initial[node.lineage] += 1
        require(sum(alive_descendants.values()) + sum(surviving_initial.values()) == state["population"], f"{prefix}: descendant/cohort population mismatch")
        require(len(self.roots) - len(self.root_death_tick) == sum(surviving_initial.values()), f"{prefix}: initial-cohort survival mismatch")
        root_aggregates = defaultdict(Counter)
        lineage_descendant_values = defaultdict(list)
        final = horizon == self.horizons[-1]
        for ident, root in self.roots.items():
            lineage = root["lineage"]
            agg = root_aggregates[lineage]
            alive_count = alive_descendants[ident]
            lineage_descendant_values[lineage].append(alive_count)
            survivor = ident in self.alive
            agg["initial_roots_at_risk"] += 1
            agg["initial_roots_surviving"] += survivor
            agg["initial_roots_with_births"] += self.root_direct_births[ident] > 0
            agg["initial_roots_with_extant_descendants"] += alive_count > 0
            agg["dead_initial_roots_with_extant_descendants"] += not survivor and alive_count > 0
            agg["direct_births_from_initial_roots"] += self.root_direct_births[ident]
            agg["living_descendants_excluding_initial_roots"] += alive_count
            agg["max_birth_depth"] = max(agg["max_birth_depth"], self.root_max_depth[ident])
            if final:
                self.final_cohort_rows.append({"arm": self.directory.name, "horizon": horizon,
                    "initial_id": ident, "lineage": lineage, "initial_parent": root["parent"],
                    "initial_born": root["born"], "initial_age": root["age"],
                    "initial_body": root["body"], "initial_store": root["store"], "initial_energy": root["energy"],
                    "initial_survives": int(survivor), "initial_death_tick": self.root_death_tick.get(ident, ""),
                    "direct_births": self.root_direct_births[ident], "all_descendant_births": self.root_births[ident],
                    "descendant_deaths": self.root_descendant_deaths[ident], "living_descendants_excluding_root": alive_count,
                    "living_descendants_after_root_death": alive_count if not survivor else 0,
                    "max_birth_depth": self.root_max_depth[ident], "max_living_descendant_depth": alive_descendant_depth[ident]})
        all_lineages = sorted(set(self.initial_lineages) | set(pop_by_lineage))
        resources_by_lineage = {row["lineage"]: row for row in state["lineages"]}
        for lineage in all_lineages:
            n0, n = self.initial_lineages[lineage], pop_by_lineage[lineage]
            require(n0 + self.births[lineage] - self.deaths[lineage] == n, f"{prefix}: lineage {lineage} flow mismatch")
            agg = root_aggregates[lineage]
            require(agg["living_descendants_excluding_initial_roots"] + agg["initial_roots_surviving"] == n, f"{prefix}: lineage descendant mismatch")
            require(agg["direct_births_from_initial_roots"] <= self.births[lineage], f"{prefix}: impossible direct births")
            gene_diversity = diversity(lineage_genomes[lineage])
            desc_dist = descendant_distribution(lineage_descendant_values[lineage])
            row = {"arm": self.directory.name, "horizon": horizon, "tick": state["tick"], "lineage": lineage,
                   "initial_population": n0, "population": n, "initial_share": ratio(n0, len(self.roots)),
                   "share": ratio(n, state["population"]), "births": self.births[lineage], "deaths": self.deaths[lineage],
                   "initial_cohort_deaths": self.initial_deaths[lineage], "newborn_deaths": self.newborn_deaths[lineage],
                   **dict(agg), "initial_survival_fraction": ratio(agg["initial_roots_surviving"], n0),
                   "initial_roots_with_births_fraction": ratio(agg["initial_roots_with_births"], n0),
                   "initial_roots_with_extant_descendants_fraction": ratio(agg["initial_roots_with_extant_descendants"], n0),
                   "dead_initial_roots_at_risk": self.initial_deaths[lineage],
                   "dead_initial_roots_with_extant_descendants_fraction": ratio(agg["dead_initial_roots_with_extant_descendants"], self.initial_deaths[lineage]),
                   "births_per_initial_individual": ratio(self.births[lineage], n0),
                   "root_descendants_median_nearest_rank": desc_dist["median_nearest_rank"],
                   "root_descendants_p90_nearest_rank": desc_dist["p90_nearest_rank"],
                   "root_descendants_p99_nearest_rank": desc_dist["p99_nearest_rank"],
                   "root_descendants_max": desc_dist["max"],
                   "root_descendants_gini_including_zero_roots": desc_dist["gini_including_zero_roots"],
                   "genotypes_exact": gene_diversity["count"],
                   "genotype_entropy_nats": gene_diversity["entropy_nats"],
                   "genotype_effective_shannon": gene_diversity["effective_shannon"],
                   "genotype_effective_simpson": gene_diversity["effective_simpson"]}
            # Counter omits zero-valued keys unless accessed; keep TSV schema fixed.
            for key in ("initial_roots_at_risk", "initial_roots_surviving", "initial_roots_with_births", "initial_roots_with_extant_descendants", "dead_initial_roots_with_extant_descendants", "direct_births_from_initial_roots", "living_descendants_excluding_initial_roots", "max_birth_depth"):
                row[key] = agg[key]
            for key in ("body", "store", "energy", "local_cache", "local_food"):
                row[key] = resources_by_lineage.get(lineage, {}).get(key, 0)
            self.lineage_rows.append(row)
        sample_summary = {}
        for kind, sample in sorted(self.samples.items()):
            sample_summary[kind] = {**dict(sample), "sample_only": True,
                                    "same_lineage_fraction": ratio(sample["same_lineage"], sample["recipient_resolved"])}
        agg_global = Counter()
        for aggregate in root_aggregates.values():
            for key, value in aggregate.items():
                if key == "max_birth_depth":
                    agg_global[key] = max(agg_global[key], value)
                else:
                    agg_global[key] += value
        for key in ("initial_roots_at_risk", "initial_roots_surviving", "initial_roots_with_births", "initial_roots_with_extant_descendants", "dead_initial_roots_with_extant_descendants", "direct_births_from_initial_roots", "living_descendants_excluding_initial_roots", "max_birth_depth"):
            agg_global[key] += 0
        for kind, counter in (("gift", "gifts"), ("attack", "attacks"), ("parasite", "parasites"), ("build", "builds")):
            require(self.samples[kind]["rows"] <= counters[counter], f"{prefix}: sampled rows exceed exhaustive counter {counter}")
        grid_rows = [{"tick": self.initial["tick"], "population": self.initial["population"]}] + [row for tick, row in sorted(self.series_rows.items()) if self.initial["tick"] < tick <= state["tick"] and tick % self.initial["config"]["sample"] == 0]
        expected_grid_count = state["tick"] // self.initial["config"]["sample"] - self.initial["tick"] // self.initial["config"]["sample"]
        require(len(grid_rows) == expected_grid_count + 1, f"{prefix}: regular series sample grid incomplete")
        grid_pops = [row["population"] for row in grid_rows]
        capacity = state["config"]["capacity"]
        result = {"horizon": horizon, "tick": state["tick"], "target_tick": target_tick,
                  "effective_tick": state["tick"], "absorbing_extinction_extension": state["tick"] < target_tick,
                  "population": state["population"],
                  "capacity": state["config"]["capacity"], "population_at_capacity": state["population"] == state["config"]["capacity"],
                  "extinct": state["population"] == 0, "counter_deltas_exhaustive": counters,
                  "population_sample_grid_predeclared": {
                      "definition": "Initial census plus actual regular series samples (tick divisible by config.sample); off-grid endpoint samples excluded. No samples invented after early extinction.",
                      "sample_period_ticks": self.initial["config"]["sample"], "samples_including_initial": len(grid_rows),
                      "observed_through_tick": state["tick"], "minimum": min(grid_pops), "maximum": max(grid_pops),
                      "mean_sampled_population": sum(grid_pops) / len(grid_pops),
                      "sampled_ticks_below_capacity": sum(pop < capacity for pop in grid_pops),
                      "mean_sampled_capacity_deficit": sum(capacity - pop for pop in grid_pops) / len(grid_pops),
                      "maximum_sampled_capacity_deficit": capacity - min(grid_pops)},
                  "population_tick_end_secondary_refinement": {
                      "definition": "Exact final census at every post-start tick, reconstructed from exhaustive birth/death events; does not measure within-tick vacancies.",
                      "ticks": horizon, "minimum": min(self.population_tick_histogram),
                      "maximum": max(self.population_tick_histogram),
                      "mean": sum(pop * duration for pop, duration in self.population_tick_histogram.items()) / horizon,
                      "ticks_at_capacity": self.population_tick_histogram[state["config"]["capacity"]],
                      "ticks_below_capacity": sum(duration for pop, duration in self.population_tick_histogram.items() if pop < state["config"]["capacity"]),
                      "fraction_ticks_at_capacity": self.population_tick_histogram[state["config"]["capacity"]] / horizon},
                  "initial_root_descendant_distribution": descendant_distribution(list(alive_descendants[ident] for ident in self.roots)),
                  "renewal_births_per_initial_population": ratio(counters["births"], len(self.roots)),
                  "initial_cohort": {**dict(agg_global),
                      "survival_fraction": ratio(agg_global["initial_roots_surviving"], len(self.roots)),
                      "with_births_fraction": ratio(agg_global["initial_roots_with_births"], len(self.roots)),
                      "with_extant_descendants_fraction": ratio(agg_global["initial_roots_with_extant_descendants"], len(self.roots)),
                      "dead_roots_at_risk": len(self.root_death_tick),
                      "dead_roots_with_extant_descendants_fraction": ratio(agg_global["dead_initial_roots_with_extant_descendants"], len(self.root_death_tick))},
                  "lineage_diversity": diversity(pop_by_lineage), "genotype_diversity_exact": diversity(genomes),
                  "resources": state["resources"], "matter": state["matter"],
                  "caches": state["caches"], "owner_trace": state.get("owner_trace", {}),
                  "cache_fraction_of_matter": ratio(state["resources"]["cache"], state["matter"]),
                  "sampled_events": sample_summary, "rng_hex": state["rng_hex"],
                  "checks": {"matter_conserved": True, "complete_birth_death_flow": True,
                      "living_ids_and_genomes_match_export": True, "lineage_flows_match_export": True,
                      "prospective_cohort_partition_exact": True, "founder_events": 0}}
        self.snapshots[str(horizon)] = result
        return result

    def run(self):
        for name in ["initial.json", "initial.agents.tsv", "events.tsv", "series.tsv"] + [f"at{h}{suffix}" for h in self.horizons for suffix in (".json", ".agents.tsv")]:
            self.hashes[name] = sha256(self.directory / name)
        with (self.directory / "events.tsv").open(newline="") as stream:
            reader = csv.DictReader(stream, delimiter="\t")
            require(REQUIRED_EVENT_FIELDS <= set(reader.fieldnames or []), f"{self.directory}: wrong events header")
            pending = next(reader, None)
            for horizon in self.horizons:
                tick_end = self.initial["tick"] + horizon
                while pending is not None and int(pending["tick"]) <= tick_end:
                    if int(pending["tick"]) <= self.initial["tick"]:
                        self.ignored_preinitial_rows += 1
                    else:
                        self.event(pending)
                    pending = next(reader, None)
                self.snapshot(horizon)
            require(pending is None, f"{self.directory}: events exceed last prespecified horizon")
        return {"initial": self.initial, "initial_lineage_diversity": diversity(self.initial_lineages),
                "initial_genotype_diversity_exact": diversity(Counter(row["genome_hex"] for row in self.roots.values())),
                "ignored_preinitial_event_rows": self.ignored_preinitial_rows, "input_sha256": self.hashes,
                "horizons": self.snapshots}


def atomic_json(path, value):
    with tempfile.NamedTemporaryFile("w", dir=path.parent, prefix=path.name + ".", delete=False) as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")
        temporary = Path(stream.name)
    temporary.replace(path)


def atomic_tsv(path, rows):
    fields = list(rows[0]) if rows else ["arm", "horizon"]
    with tempfile.NamedTemporaryFile("w", dir=path.parent, prefix=path.name + ".", newline="", delete=False) as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow({key: "" if value is None else value for key, value in row.items()})
        temporary = Path(stream.name)
    temporary.replace(path)


def compare_row(arm, horizon, data, control):
    def extract(state):
        counters = state["counter_deltas_exhaustive"]
        cohort = state["initial_cohort"]
        return {"population": state["population"], "births": counters["births"], "deaths": counters["deaths"],
                "gifts": counters["gifts"], "attacks": counters["attacks"], "parasites": counters["parasites"],
                "initial_survivors": cohort["initial_roots_surviving"],
                "initial_roots_with_births": cohort["initial_roots_with_births"],
                "initial_roots_with_extant_descendants": cohort["initial_roots_with_extant_descendants"],
                "living_descendants_excluding_initial_roots": cohort["living_descendants_excluding_initial_roots"],
                "dead_initial_roots_with_extant_descendants": cohort["dead_initial_roots_with_extant_descendants"],
                "lineages": state["lineage_diversity"]["count"],
                "lineage_effective_shannon": state["lineage_diversity"]["effective_shannon"],
                "genotypes_exact": state["genotype_diversity_exact"]["count"],
                "genotype_effective_shannon": state["genotype_diversity_exact"]["effective_shannon"],
                "cache": state["resources"]["cache"], "food": state["resources"]["food"],
                "mineral": state["resources"]["mineral"], "body": state["resources"]["body"], "store": state["resources"]["store"],
                "cache_gini_all_cells": state["caches"]["gini_all_cells"],
                "cache_top1pct_share_all_cells": state["caches"]["top1pct_share_all_cells"]}
    values, refs = extract(data), extract(control)
    row = {"arm": arm, "horizon": horizon, "tick": data["tick"]}
    for name, value in values.items():
        row[name] = value
        row[name + "_delta_control"] = value - refs[name] if value is not None and refs[name] is not None else None
        row[name + "_relative_delta_control"] = ratio(value - refs[name], refs[name]) if value is not None and refs[name] is not None else None
    return row


def analyze(window_dir, horizons):
    require(horizons and horizons == sorted(set(horizons)) and horizons[0] > 0, "horizons must be sorted distinct positive integers")
    output = {"schema": "relief-comparatif-analysis-v1", "window": window_dir.name, "prespecified_horizons": horizons,
              "definitions": {
                  "cohort_root": "Every initial living ID is a distinct prospective root; follow post-start parent links until an initial ID is encountered. Existing parent links between initial IDs do not merge roots.",
                  "descendants": "Exclude the initial root itself; retain lineage attribution after the root dies.",
                  "at_risk": "Initial living individuals, by lineage; report denominators even for tiny lineages.",
                  "diversity": "Full 64-byte genome identity (not hash identity); Shannon entropy in nats; Hill effective counts; exact snapshot census.",
                  "interactions": "Global deltas from cumulative checkpoint counters are exhaustive. Logged gift/attack/parasite/deposit/build events are only a deterministic 1/256 actor-tick-kind sample; do not extrapolate or infer independent observations.",
                  "causality": "Paired deterministic trajectories from one checkpoint are conditional interventions, not independent replications. Additional initial windows test temporal robustness, not independent RNG seeds.",
                  "extinction": "The exact ELF stops when population becomes zero. An early-extinct export may have effective_tick < target_tick: population and genealogy are then carried as absorbing zero, while physical state/counters/RNG must remain exactly the extinct checkpoint; no founders or continuing world are created.",
                  "cohort_mean_warning": "When all initial IDs died, mean living descendants per initial ID equals final population / initial population by construction. Extant-root fraction, distribution, quantiles and lineage shares carry additional information.",
                  "descendant_quantiles": "Nearest-rank census quantiles across all initial roots including zeros; top 1% uses ceil(number of roots / 100).",
                  "population_tick_end": "Secondary refinement specified before outcome inspection: reconstruct exact census after every tick from complete births/deaths; keep distinct from predeclared 100-tick series samples and within-tick vacancies.",
                  "uncertainty": "No organism-level statistical confidence intervals: shared environment and descent violate independence. Census values are exact for the observed finite trajectories."},
              "arms": {}}
    analyses = {}
    for arm in ARMS:
        analysis = ArmAnalysis(window_dir / arm, horizons)
        output["arms"][arm] = analysis.run()
        analyses[arm] = analysis
    # All arms must start with identical living agents, counters, config and RNG.
    reference = analyses["control"]
    for arm, analysis in analyses.items():
        require(analysis.roots == reference.roots, f"{arm}: initial living-agent records differ")
        for key in ("tick", "rng_hex", "initial_matter", "matter", "config", "counters"):
            require(analysis.initial[key] == reference.initial[key], f"{arm}: initial {key} differs")
    output["paired_start_checks"] = {"identical_initial_agents": True, "identical_rng_and_counters": True,
                                      "identical_total_matter_and_config": True}
    comparison = [compare_row(arm, horizon, analyses[arm].snapshots[str(horizon)], reference.snapshots[str(horizon)]) for horizon in horizons for arm in ARMS]
    atomic_json(window_dir / "analysis.json", output)
    atomic_tsv(window_dir / "lineage-results.tsv", [row for arm in ARMS for row in analyses[arm].lineage_rows])
    atomic_tsv(window_dir / "cohort-results.tsv", [row for arm in ARMS for row in analyses[arm].final_cohort_rows])
    atomic_tsv(window_dir / "comparison.tsv", comparison)
    return output


def selftest():
    """Independent tiny fixture: prospective roots, death, depth, extinction."""
    fields = ["id", "parent", "lineage", "born", "age", "energy", "body", "store", "genotype_hash", "genome_hex"]
    event_fields = ["tick", "kind", "id", "lineage", "other", "position", "amount", "genotype_hash", "genome_hex"]
    genome = "00" * 64
    agents = {ident: dict(zip(fields, [ident, parent, lineage, born, 1, 100, 10, 0, "fixture-hash", genome]))
              for ident, parent, lineage, born in [(1, 0, 1, 90), (2, 0, 2, 95), (3, 1, 1, 98), (4, 1, 1, 101), (5, 4, 1, 102)]}
    events = [(101, "birth", 4, 1, 1), (102, "death", 1, 1, 0),
              (102, "birth", 5, 1, 4), (103, "gift", 5, 1, 2),
              (104, "death", 4, 1, 0), (105, "death", 2, 2, 0),
              (105, "death", 3, 1, 0), (105, "death", 5, 1, 0)]
    def fixture_state(tick, ids, births, deaths, gifts):
        counters = dict.fromkeys(COUNTERS, 0)
        counters.update(births=births, deaths=deaths, gifts=gifts)
        resources = dict(mineral=0, food=0, detritus=100 - 10 * len(ids), structure=0, cache=0,
                         body=10 * len(ids), store=0, energy=100 * len(ids))
        counts = Counter(agents[ident]["lineage"] for ident in ids)
        return dict(tick=tick, population=len(ids), rng_hex="fixture", initial_matter=100, matter=100,
                    config=dict(capacity=5, sample=1), counters=counters, resources=resources,
                    diversity=dict(genotypes_exact=int(bool(ids))),
                    caches=dict(gini_all_cells=0, top1pct_share_all_cells=0),
                    lineages=[dict(lineage=key, population=count) for key, count in counts.items()])
    with tempfile.TemporaryDirectory(prefix="relief-analysis-selftest-") as temporary:
        window = Path(temporary) / "window"
        window.mkdir()
        for arm in ARMS:
            directory = window / arm
            directory.mkdir()
            for name, tick, ids, births, deaths, gifts in [
                    ("initial", 100, [1, 2, 3], 0, 0, 0),
                    ("at1", 101, [1, 2, 3, 4], 1, 0, 0),
                    ("at4", 104, [2, 3, 5], 2, 2, 1),
                    ("at8", 105, [], 2, 5, 1)]:
                (directory / (name + ".json")).write_text(json.dumps(fixture_state(tick, ids, births, deaths, gifts)))
                with (directory / (name + ".agents.tsv")).open("w", newline="") as stream:
                    writer = csv.DictWriter(stream, fieldnames=fields, delimiter="\t")
                    writer.writeheader()
                    writer.writerows(agents[ident] for ident in ids)
            with (directory / "events.tsv").open("w", newline="") as stream:
                writer = csv.writer(stream, delimiter="\t")
                writer.writerow(event_fields)
                for tick, kind, ident, lineage, other in events:
                    writer.writerow([tick, kind, ident, lineage, other, 0, 1, "fixture-hash", genome if kind == "birth" else ""])
            with (directory / "series.tsv").open("w", newline="") as stream:
                writer = csv.writer(stream, delimiter="\t")
                writer.writerow(["tick", "population", "births", "deaths"])
                writer.writerows([(100, 3, 0, 0), (101, 4, 1, 0), (102, 4, 2, 1),
                                 (103, 4, 2, 1), (104, 3, 2, 2), (105, 0, 2, 5)])
        output = analyze(window, [1, 4, 8])
        horizons = output["arms"]["control"]["horizons"]
        cohort = horizons["4"]["initial_cohort"]
        assert cohort["initial_roots_surviving"] == 2
        assert cohort["dead_initial_roots_with_extant_descendants"] == 1
        assert cohort["living_descendants_excluding_initial_roots"] == 1
        assert cohort["max_birth_depth"] == 2
        assert cohort["direct_births_from_initial_roots"] == 1
        assert horizons["4"]["sampled_events"]["gift"]["same_lineage_fraction"] == 0
        assert horizons["4"]["population_tick_end_secondary_refinement"]["mean"] == 3.75
        assert horizons["4"]["initial_root_descendant_distribution"]["max"] == 1
        assert horizons["8"]["extinct"] and horizons["8"]["absorbing_extinction_extension"]
        assert horizons["8"]["target_tick"] == 108 and horizons["8"]["effective_tick"] == 105
        assert horizons["8"]["population_tick_end_secondary_refinement"]["mean"] == 1.875
        assert horizons["8"]["genotype_diversity_exact"]["count"] == 0
        with (window / "cohort-results.tsv").open() as stream:
            rows = list(csv.DictReader(stream, delimiter="\t"))
        assert len(rows) == 9 and all(row["living_descendants_excluding_root"] == "0" for row in rows)
        # The initial individual 3 is its own root despite initial parent=1.
        lineage_one = [row for row in output["arms"]["control"]["initial"]["lineages"] if row["lineage"] == 1]
        assert lineage_one[0]["population"] == 2
        path = window / "control" / "events.tsv"
        path.write_text(path.read_text().replace("101\tbirth", "101\tfounder", 1))
        try:
            analyze(window, [1, 4, 8])
        except ValueError as error:
            assert "forbidden founder event" in str(error)
        else:
            raise AssertionError("founder event accepted")
    print("PASS: prospective cohorts, initial-parent separation, root death with extant descendants, birth depth, sampled gift lineage, exact tick-end population, early absorbing extinction, no founders")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("window_dir", type=Path, nargs="?")
    parser.add_argument("--selftest", action="store_true", help="Run reproducible bounded genealogy/extinction fixture")
    parser.add_argument("--horizons", default="600,4096,8192", help="Prespecified cumulative tick horizons")
    args = parser.parse_args()
    if args.selftest:
        selftest()
        return
    if args.window_dir is None:
        parser.error("window_dir is required unless --selftest")
    horizons = [int(item) for item in args.horizons.split(",")]
    output = analyze(args.window_dir.resolve(), horizons)
    print(json.dumps({"window": output["window"], "arms": list(output["arms"]), "horizons": horizons, "checks": "PASS", "output": str(args.window_dir / "analysis.json")}, ensure_ascii=False))


if __name__ == "__main__":
    main()
