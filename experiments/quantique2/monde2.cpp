// Q2 : ensembles qui se recopient par l'observation. Suite de Q1 (experiments/quantique), voir PROTOCOLE.md.
// Même noyau U, même champ de phase unitaire, même projection de Born. Trois changements :
//  1. bain thermique (Langevin) : les liens se font et se défont, le réseau ne percole plus d'office ;
//  2. identité persistante d'un ensemble (recouvrement d'une étape à l'autre), naissances et filiation ;
//  3. la projection a une conséquence : la valeur tirée est écrite dans l'octet libre le plus proche
//     (empreinte). Témoins : empreinte éteinte (0) ou valeur tirée au hasard, indépendante du champ (2).
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
using C = std::complex<double>;
constexpr double pi = 3.14159265358979323846, dt = .02, cutoff = 2.5;
using V = std::array<double, 3>;
V operator+(V a, V b) { for (int k = 0; k < 3; k++) a[k] += b[k]; return a; }
V operator-(V a, V b) { for (int k = 0; k < 3; k++) a[k] -= b[k]; return a; }
V operator*(V a, double b) { for (auto &x : a) x *= b; return a; }
double norm(V a) { return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }
struct Kernel { double u, du; };
Kernel kernel(double r) {                       // identique à Q1
    if (r >= cutoff) return {0, 0};
    double e = std::exp(.9 - r), u = e * e - 2 * e, du = 2 * (e - e * e);
    double z = 1 - r * r / (cutoff * cutoff), w = z * z, dw = -4 * r * z / (cutoff * cutoff);
    return {u * w, du * w + u * dw};
}
// Queue répulsive, indépendante des valeurs : A·exp(-r/ξ)/r, coupée continûment à rcut.
Kernel tail(double r, double A, double xi, double rc) {
    if (A == 0 || r >= rc) return {0, 0};
    double y = A * std::exp(-r / xi) / r, dy = -y * (1 / xi + 1 / r);
    double z = 1 - r * r / (rc * rc), w = z * z, dw = -4 * r * z / (rc * rc);
    return {y * w, dy * w + y * dw};
}
void fft(std::vector<C> &a, bool inverse) {
    size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        C wlen = std::polar(1.0, (inverse ? 2 : -2) * pi / len);
        for (size_t i = 0; i < n; i += len) { C w = 1; for (size_t j = 0; j < len / 2; j++) {
            C u = a[i + j], v = a[i + j + len / 2] * w;
            a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wlen;
        } }
    }
    if (inverse) for (auto &x : a) x /= n;
}
// distance circulaire entre deux valeurs d'octet (0..128)
double cdist(double a, double b) { double d = std::fmod(std::fabs(a - b), 256.0); return d > 128 ? 256 - d : d; }

struct Byte { V x{}, v{}; uint32_t value = 0; uint64_t id = 0, mark = 0, mark_t = 0; };
struct Site { std::vector<C> psi; uint64_t owner = 0, streak = 0; };
struct Lineage { uint64_t parent = 0, gen = 0, born = 0, last = 0; double phase = 0; uint32_t kind = 0; bool alive = true; };
template <class T> void put(std::ostream &o, const T &v) { o.write(reinterpret_cast<const char *>(&v), sizeof v); }
template <class T> void get(std::istream &i, T &v) { i.read(reinterpret_cast<char *>(&v), sizeof v); if (!i) throw std::runtime_error("checkpoint tronqué"); }

struct Params {
    uint64_t seed = 101, period = 16, imprint = 1, window = 1024;
    uint64_t n = 96, grid = 6, modes = 64, bind_age = 8;
    double box = 12, gamma = .5, temp = .1, bind_r = 1.4, gain = 1;
    double rep = 0, xi = 2, rcut = 5;   // queue répulsive à longue portée (SALR), 0 = noyau Q1 seul
    double spec = .5;                  // affinité 1 + spec·cos(Δvaleur) ; 0,5 = Q1
    double cool = 0;                   // relaxation du champ par le bain : psi ×= exp(-dt·cool·V), puis renormalisation ; 0 = unitaire (Q1)
};

struct World {
    Params P;
    uint64_t rng = 0, rng2 = 0, t = 0, events = 0, imprints = 0, next_id = 1;
    uint64_t births[3] = {0, 0, 0}, deaths = 0, maxgen = 0;
    std::vector<Byte> bytes;
    std::vector<Site> sites;
    std::vector<uint64_t> ages;
    std::map<uint64_t, Lineage> lin;
    std::vector<double> cs, sn;
    std::vector<C> freephase;
    // dernier relevé
    size_t ensembles = 0, largest = 0, in_ens = 0; double order = 0; uint64_t gen_alive = 0;
    static uint64_t mix(uint64_t &s) { uint64_t z = (s += 0x9e3779b97f4a7c15ULL); z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL; z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL; return z ^ (z >> 31); }
    uint64_t next() { return mix(rng); }
    double uniform() { return (next() >> 11) * 0x1.0p-53; }
    double gauss() { double a = uniform(), b = uniform(); if (a < 1e-300) a = 1e-300; return std::sqrt(-2 * std::log(a)) * std::cos(2 * pi * b); }
    double u2() { return (mix(rng2) >> 11) * 0x1.0p-53; }  // flux séparé : témoin et ligne de base
    V delta(V a, V b) const { V d = b - a; for (auto &x : d) x -= P.box * std::round(x / P.box); return d; }
    void tables() {
        int m = (int)P.modes; cs.resize(m); sn.resize(m); freephase.resize(m);
        for (int j = 0; j < m; j++) {
            cs[j] = std::cos(2 * pi * j / m); sn[j] = std::sin(2 * pi * j / m);
            int k = j <= m / 2 ? j : j - m;
            freephase[j] = std::polar(1.0, -dt * k * k / 50.0);
        }
    }
    void init() {
        tables(); rng = P.seed; rng2 = P.seed ^ 0x51325132ULL; bytes.assign(P.n, Byte{}); ages.assign(P.n * P.n, 0);
        for (auto &b : bytes) {
            b.value = next() % 256;
            for (auto &x : b.x) x = uniform() * P.box;
            for (auto &v : b.v) v = (uniform() - .5) * .15;
        }
        sites.assign(P.grid * P.grid * P.grid, Site{});
        for (auto &s : sites) s.psi.assign(P.modes, C(1 / std::sqrt(double(P.modes)), 0));
    }
    V position(size_t i) const {
        size_t g = P.grid; double h = P.box / g;
        return {(double(i % g) + .5) * h, (double((i / g) % g) + .5) * h, (double(i / (g * g)) + .5) * h};
    }
    std::array<double, 2> moment(const Site &s) const {
        std::array<double, 2> m{};
        for (size_t j = 0; j < P.modes; j++) { double p = std::norm(s.psi[j]); m[0] += p * cs[j]; m[1] += p * sn[j]; } return m;
    }
    void evolve(Site &s, double vc, double vs) {
        for (size_t j = 0; j < P.modes; j++) s.psi[j] *= std::polar(1.0, -dt * P.gain * (vc * cs[j] + vs * sn[j]));
        fft(s.psi, false); for (size_t j = 0; j < P.modes; j++) s.psi[j] *= freephase[j]; fft(s.psi, true);
        if (P.cool > 0 && (vc != 0 || vs != 0)) {
            double z = 0;
            for (size_t j = 0; j < P.modes; j++) { s.psi[j] *= std::exp(-dt * P.cool * P.gain * (vc * cs[j] + vs * sn[j])); z += std::norm(s.psi[j]); }
            z = 1 / std::sqrt(z); for (auto &x : s.psi) x *= z;
        }
    }
    int project(Site &s) {
        double sum = 0; for (auto z : s.psi) sum += std::norm(z);
        double target = uniform() * sum, acc = 0; int chosen = (int)P.modes - 1;
        for (size_t j = 0; j < P.modes; j++) { acc += std::norm(s.psi[j]); if (target < acc) { chosen = (int)j; break; } }
        std::fill(s.psi.begin(), s.psi.end(), C(0)); s.psi[chosen] = 1; return chosen;
    }
    // phase (valeur moyenne circulaire, 0..256) et cohérence R d'un ensemble de valeurs
    static std::pair<double, double> phase_of(const std::vector<uint32_t> &vals) {
        double c = 0, s = 0; for (auto v : vals) { c += std::cos(2 * pi * v / 256); s += std::sin(2 * pi * v / 256); }
        double ph = std::atan2(s, c) / (2 * pi) * 256; if (ph < 0) ph += 256;
        return {ph, vals.empty() ? 0 : std::sqrt(c * c + s * s) / vals.size()};
    }
    void step(std::ostream *ev = nullptr, std::ostream *bl = nullptr) {
        size_t n = bytes.size(); std::vector<V> forces(n, V{});
        std::vector<size_t> parent(n); std::iota(parent.begin(), parent.end(), 0);
        auto root = [&](size_t a) { while (a != parent[a]) { parent[a] = parent[parent[a]]; a = parent[a]; } return a; };
        for (size_t i = 0; i < n; i++) for (size_t j = i + 1; j < n; j++) {
            V d = delta(bytes[i].x, bytes[j].x); double r = std::sqrt(norm(d) * norm(d) + .01);
            auto k = kernel(r); double a = 1 + P.spec * std::cos(2 * pi * (double(bytes[i].value) - bytes[j].value) / 256);
            auto q = tail(r, P.rep, P.xi, P.rcut);
            V f = d * ((a * k.du + q.du) / r); forces[i] = forces[i] + f; forces[j] = forces[j] - f;
            uint64_t &age = ages[i * n + j]; if (r < P.bind_r) ++age; else age = 0;
            if (age >= P.bind_age) { size_t u = root(i), v = root(j); if (u != v) parent[v] = u; }
        }
        // composantes et identités persistantes
        std::vector<std::vector<size_t>> comp(n);
        for (size_t i = 0; i < n; i++) comp[root(i)].push_back(i);
        struct Cand { size_t c; uint64_t id; size_t cnt; };
        std::vector<Cand> cands; std::vector<uint64_t> cid(n, 0);
        for (size_t c = 0; c < n; c++) if (comp[c].size() >= 2) {
            std::map<uint64_t, size_t> tally; for (auto i : comp[c]) if (bytes[i].id) ++tally[bytes[i].id];
            for (auto &[id, cnt] : tally) if (cnt >= 2) cands.push_back({c, id, cnt});
        }
        std::stable_sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) { return a.cnt != b.cnt ? a.cnt > b.cnt : a.id < b.id; });
        std::map<uint64_t, size_t> claimed;
        for (auto &k : cands) if (!cid[k.c] && !claimed.count(k.id)) { cid[k.c] = k.id; claimed[k.id] = k.c; }
        // relevé des phases des vivants (pour la ligne de base aléatoire)
        std::vector<std::pair<uint64_t, double>> living;
        ensembles = largest = in_ens = 0; gen_alive = 0; double oc = 0, os = 0;
        std::vector<uint64_t> newborn;
        std::vector<std::string> pending;
        for (size_t c = 0; c < n; c++) if (comp[c].size() >= 2) {
            std::vector<uint32_t> vals; for (auto i : comp[c]) vals.push_back(bytes[i].value);
            auto [ph, R] = phase_of(vals);
            ++ensembles; largest = std::max(largest, comp[c].size()); in_ens += comp[c].size();
            oc += std::cos(2 * pi * ph / 256); os += std::sin(2 * pi * ph / 256);
            if (!cid[c]) {
                // naissance : fission (majorité d'un ancien déjà reprise par une autre part), empreinte, ou spontanée
                uint64_t par = 0; uint32_t kind = 0;
                std::map<uint64_t, size_t> tally; for (auto i : comp[c]) if (bytes[i].id) ++tally[bytes[i].id];
                size_t best = 0; for (auto &[id, cnt] : tally) if (cnt > best) { best = cnt; par = id; }
                if (best >= 2) kind = 1;
                else {
                    par = 0; std::map<uint64_t, size_t> marks;
                    for (auto i : comp[c]) if (bytes[i].mark && t - bytes[i].mark_t <= P.window) ++marks[bytes[i].mark];
                    size_t mb = 0; for (auto &[id, cnt] : marks) if (cnt > mb) { mb = cnt; par = id; }
                    if (mb * 2 >= comp[c].size()) kind = 2; else par = 0;
                }
                uint64_t id = next_id++; cid[c] = id;
                Lineage L; L.parent = par; L.kind = kind; L.born = t; L.last = t; L.phase = ph;
                auto pit = par ? lin.find(par) : lin.end();
                L.gen = pit != lin.end() ? pit->second.gen + 1 : 0;
                lin[id] = L; ++births[kind]; maxgen = std::max(maxgen, L.gen);
                newborn.push_back(id);
                if (bl) {
                    double dpar = pit != lin.end() ? cdist(ph, pit->second.phase) : -1;
                    std::ostringstream os; os.precision(8);
                    os << t + 1 << '\t' << id << '\t' << par << '\t' << kind << '\t' << L.gen << '\t' << comp[c].size() << '\t' << ph << '\t' << R
                       << '\t' << (pit != lin.end() ? pit->second.phase : -1) << '\t' << dpar << '\t';
                    pending.push_back(os.str());   // la ligne de base est ajoutée après le relevé des vivants
                }
            }
            auto &L = lin[cid[c]]; L.phase = ph; L.last = t; L.alive = true;
            gen_alive = std::max(gen_alive, L.gen);
            living.push_back({cid[c], ph});
            for (auto i : comp[c]) bytes[i].id = cid[c];
        }
        for (size_t c = 0; c < n; c++) if (comp[c].size() == 1) bytes[comp[c][0]].id = 0;
        order = ensembles ? std::sqrt(oc * oc + os * os) / ensembles : 0;
        // ligne de base : distance du nouveau-né à un autre vivant tiré au hasard (ni lui, ni son parent)
        size_t nb_i = 0;
        for (auto id : newborn) {
            auto &L = lin[id]; std::vector<double> others;
            for (auto &[o, ph] : living) if (o != id && o != L.parent) others.push_back(ph);
            double drand = others.empty() ? -1 : cdist(L.phase, others[size_t(u2() * others.size()) % others.size()]);
            if (bl) *bl << pending[nb_i++] << drand << '\n';
        }
        // morts et ménage de la table des lignées
        for (auto it = lin.begin(); it != lin.end();) {
            auto &L = it->second;
            if (L.alive && L.last != t) { L.alive = false; ++deaths; }
            if (!L.alive && t - L.last > P.window + 8) it = lin.erase(it); else ++it;
        }
        // champ : même boucle d'attention que Q1, propriétaire = identité persistante
        for (size_t si = 0; si < sites.size(); si++) {
            Site &s = sites[si]; auto before = moment(s); V p = position(si);
            double vc = 0, vs = 0;
            std::map<uint64_t, size_t> hits, returns; std::map<uint64_t, double> strength;
            std::vector<V> derivatives(n, V{}); std::vector<double> dist(n, 1e9);
            for (size_t i = 0; i < n; i++) {
                V d = delta(bytes[i].x, p); double r = std::sqrt(norm(d) * norm(d) + .01); auto k = kernel(r); dist[i] = r;
                double a = 2 * pi * bytes[i].value / 256, co = std::cos(a), s0 = std::sin(a);
                double affinity = 1 + .5 * (co * before[0] + s0 * before[1]);
                forces[i] = forces[i] + d * (affinity * k.du / r);
                vc += .5 * k.u * co; vs += .5 * k.u * s0;
                derivatives[i] = d * (.5 * k.du / r);
                if (bytes[i].id && std::abs(k.du) > 1e-8) { ++hits[bytes[i].id]; strength[bytes[i].id] += std::abs(k.du); }
            }
            Site cf = s; evolve(cf, 0, 0);
            evolve(s, vc, vs); auto after = moment(s), without = moment(cf);
            for (size_t i = 0; i < n; i++) if (bytes[i].id) {
                double a = 2 * pi * bytes[i].value / 256;
                double da = std::cos(a) * (after[0] - without[0]) + std::sin(a) * (after[1] - without[1]);
                if (norm(derivatives[i] * da) > 1e-10) ++returns[bytes[i].id];
            }
            uint64_t owner = 0; double best = 0;
            for (auto &[id, h] : hits) if (h >= 2 && returns[id] >= 2 && strength[id] > best) { owner = id; best = strength[id]; }
            if (owner && owner == s.owner) ++s.streak; else s.streak = owner ? 1 : 0;
            s.owner = owner;
            if (P.period && s.streak >= P.period) {
                int state = project(s); ++events; s.streak = 0;
                uint32_t val = uint32_t(state * 256 / P.modes);
                long target = -1; double bd = cutoff;
                if (P.imprint) for (size_t i = 0; i < n; i++) if (!bytes[i].id && dist[i] < bd) { bd = dist[i]; target = (long)i; }
                if (target >= 0) {
                    if (P.imprint == 2) val = uint32_t(u2() * 256) % 256;   // témoin : valeur sans lien avec le champ
                    bytes[target].value = val; bytes[target].mark = owner; bytes[target].mark_t = t; ++imprints;
                }
                if (ev) *ev << t + 1 << '\t' << si << '\t' << owner << '\t' << val << '\t' << target << '\t' << lin[owner].phase << '\n';
            }
        }
        // Langevin : frottement et bruit, puis déplacement
        double sig = std::sqrt(2 * P.gamma * P.temp * dt);
        for (size_t i = 0; i < n; i++) for (int k = 0; k < 3; k++) {
            bytes[i].v[k] += dt * (forces[i][k] - P.gamma * bytes[i].v[k]) + sig * gauss();
            bytes[i].x[k] += dt * bytes[i].v[k];
            bytes[i].x[k] -= P.box * std::floor(bytes[i].x[k] / P.box);
            if (!std::isfinite(bytes[i].v[k])) throw std::runtime_error("vitesse non finie");
        }
        ++t;
    }
    double normerror() const { double e = 0; for (auto &s : sites) { double z = 0; for (auto v : s.psi) z += std::norm(v); e = std::max(e, std::abs(z - 1)); } return e; }
    double kinetic() const { double k = 0; for (auto &b : bytes) for (double v : b.v) k += .5 * v * v; return k / std::max<size_t>(1, bytes.size()); }
    static const char *header() { return "t\tensembles\tplus_grand\toctets_lies\tordre_phase\tnaissances_spont\tnaissances_fission\tnaissances_empreinte\tmorts\tgen_max_vivante\tgen_max\tprojections\tempreintes\terreur_norme\tenergie_cin\n"; }
    void metrics(std::ostream &o) const {
        o << t << '\t' << ensembles << '\t' << largest << '\t' << in_ens << '\t' << order << '\t' << births[0] << '\t' << births[1] << '\t' << births[2]
          << '\t' << deaths << '\t' << gen_alive << '\t' << maxgen << '\t' << events << '\t' << imprints << '\t' << normerror() << '\t' << kinetic() << '\n';
    }
    uint64_t hash() const {
        uint64_t h = 1469598103934665603ULL; auto add = [&](const void *p, size_t len) { auto c = (const unsigned char *)p; for (size_t i = 0; i < len; i++) h = (h ^ c[i]) * 1099511628211ULL; };
        for (auto &b : bytes) { add(&b.x, sizeof b.x); add(&b.v, sizeof b.v); add(&b.value, 4); add(&b.id, 8); add(&b.mark, 8); }
        for (auto &s : sites) add(s.psi.data(), s.psi.size() * sizeof(C));
        add(&rng, 8); add(&rng2, 8); add(&events, 8); add(births, sizeof births); return h;
    }
    void save(const std::filesystem::path &p) const {
        std::ofstream o(p.string() + ".tmp", std::ios::binary); o.exceptions(std::ios::badbit | std::ios::failbit);
        uint64_t magic = 0x5132574f524c4401ULL; put(o, magic); put(o, P);
        put(o, rng); put(o, rng2); put(o, t); put(o, events); put(o, imprints); put(o, next_id); put(o, births); put(o, deaths); put(o, maxgen);
        put(o, ensembles); put(o, largest); put(o, in_ens); put(o, order); put(o, gen_alive);
        for (auto &b : bytes) put(o, b);
        for (auto a : ages) put(o, a);
        for (auto &s : sites) { put(o, s.owner); put(o, s.streak); for (auto z : s.psi) { double re = z.real(), im = z.imag(); put(o, re); put(o, im); } }
        uint64_t nl = lin.size(); put(o, nl); for (auto &[id, L] : lin) { put(o, id); put(o, L); }
        o.close(); std::filesystem::rename(p.string() + ".tmp", p);
    }
    void load(const std::filesystem::path &p) {
        std::ifstream i(p, std::ios::binary); uint64_t magic; get(i, magic);
        if (magic != 0x5132574f524c4401ULL) throw std::runtime_error("version checkpoint incorrecte");
        get(i, P);
        if (P.n > 1024 || P.grid < 1 || P.grid > 12 || P.modes < 16 || P.modes > 256 || (P.modes & (P.modes - 1))) throw std::runtime_error("dimensions checkpoint invalides");
        get(i, rng); get(i, rng2); get(i, t); get(i, events); get(i, imprints); get(i, next_id); get(i, births); get(i, deaths); get(i, maxgen);
        get(i, ensembles); get(i, largest); get(i, in_ens); get(i, order); get(i, gen_alive);
        tables(); bytes.resize(P.n); ages.resize(P.n * P.n); sites.resize(P.grid * P.grid * P.grid);
        for (auto &b : bytes) get(i, b);
        for (auto &a : ages) get(i, a);
        for (auto &s : sites) { get(i, s.owner); get(i, s.streak); s.psi.resize(P.modes); for (auto &z : s.psi) { double re, im; get(i, re); get(i, im); z = {re, im}; } }
        uint64_t nl; get(i, nl); lin.clear(); for (uint64_t k = 0; k < nl; k++) { uint64_t id; Lineage L; get(i, id); get(i, L); lin[id] = L; }
        if (normerror() > 1e-7) throw std::runtime_error("norme checkpoint invalide");
    }
};
void require(bool p, const char *msg) { if (!p) throw std::runtime_error(msg); }
void recover_log(const std::filesystem::path &p, uint64_t tick) {
    if (!std::filesystem::exists(p)) throw std::runtime_error("journal manquant pour reprise");
    std::ifstream in(p); std::ofstream out(p.string() + ".recover"); out.exceptions(std::ios::badbit | std::ios::failbit);
    std::string line; bool header = true;
    while (std::getline(in, line)) {
        if (header) { out << line << '\n'; header = false; continue; }
        auto tab = line.find('\t'); if (tab == std::string::npos) continue;
        if (std::count(line.begin(), line.end(), '\t') < 2) continue;
        try { if (std::stoull(line.substr(0, tab)) <= tick) out << line << '\n'; } catch (const std::exception &) { continue; }
    }
    out.close(); in.close(); std::filesystem::rename(p.string() + ".recover", p);
}
void selftest() {
    require(kernel(.3).du < 0 && kernel(1.5).du > 0, "signes force");
    require(kernel(cutoff).u == 0 && kernel(cutoff).du == 0, "coupure");
    for (double r : {.5, 1.5, 3.0, 4.9}) require(std::abs((tail(r + 1e-6, .3, 2, 5).u - tail(r - 1e-6, .3, 2, 5).u) / 2e-6 - tail(r, .3, 2, 5).du) < 1e-6, "gradient queue");
    require(tail(5, .3, 2, 5).u == 0 && tail(3, .3, 2, 5).du < 0, "queue repulsive et coupee");
    require(std::abs(cdist(250, 5) - 11) < 1e-12 && std::abs(cdist(0, 128) - 128) < 1e-12, "distance circulaire");
    World u; u.P.n = 2; u.init(); Site s = u.sites[0]; for (int j = 0; j < 200; j++) u.evolve(s, 1, .5);
    double z = 0; for (C x : s.psi) z += std::norm(x); require(std::abs(z - 1) < 1e-10, "unitarite");
    // Le champ seul, sous le potentiel d'un octet de valeur a, se concentre vers a : c'est ce que l'empreinte recopie.
    {
        World w; w.P.n = 1; w.P.grid = 1; w.P.box = 4; w.P.temp = 0; w.P.period = 0; w.init();
        w.bytes[0].x = {2.9, 2, 2}; w.bytes[0].v = {}; w.bytes[0].value = 64;   // site au centre (2,2,2), r = 0,9
        Site s0 = w.sites[0]; for (int j = 0; j < 400; j++) { auto k = kernel(std::sqrt(.81 + .01)); w.evolve(s0, .5 * k.u * std::cos(pi / 2), .5 * k.u * std::sin(pi / 2)); }
        auto m = w.moment(s0); double ph = std::atan2(m[1], m[0]) / (2 * pi) * 256; if (ph < 0) ph += 256;
        require(cdist(ph, 64) < 16 && std::hypot(m[0], m[1]) > .05, "champ oriente vers la valeur de l'octet");
    }
    // Déterminisme et reprise exacte, avec empreintes et naissances.
    auto dir = std::filesystem::temp_directory_path() / ("q2-selftest-" + std::to_string(getpid()));
    std::filesystem::create_directories(dir);
    World a; a.P.n = 48; a.P.box = 8; a.P.grid = 4; a.P.period = 4; a.init(); for (int j = 0; j < 600; j++) a.step();
    World b; b.P = a.P; b.init(); for (int j = 0; j < 300; j++) b.step(); b.save(dir / "s.ck");
    World c; c.load(dir / "s.ck"); for (int j = 0; j < 300; j++) c.step();
    require(a.hash() == c.hash() && a.births[0] + a.births[1] + a.births[2] == c.births[0] + c.births[1] + c.births[2], "reprise exacte");
    require(a.events > 0, "projections");
    require(a.imprints > 0, "empreintes");
    // Empreinte : jamais sur un octet déjà dans un ensemble ; témoin 0 : aucune écriture.
    World off = World(); off.P = a.P; off.P.imprint = 0; off.init(); std::vector<uint32_t> v0; for (auto &x : off.bytes) v0.push_back(x.value);
    for (int j = 0; j < 600; j++) off.step();
    bool same = true; for (size_t i = 0; i < v0.size(); i++) same &= v0[i] == off.bytes[i].value;
    require(same && off.imprints == 0, "temoin sans empreinte");
    World ctl; ctl.P = a.P; ctl.P.imprint = 2; ctl.init(); for (int j = 0; j < 600; j++) ctl.step();
    require(ctl.imprints > 0, "temoin aleatoire ecrit");
    std::filesystem::remove_all(dir);
    std::cout << "selftest q2: force, coupure, distance, unitarite, champ oriente, reprise exacte, empreintes, temoins OK\n";
}
int main(int argc, char **argv) { try {
    if (argc == 2 && std::string(argv[1]) == "selftest") { selftest(); return 0; }
    if (argc < 3 || std::string(argv[1]) != "run")
        throw std::runtime_error("usage: monde2 run DIR [--ticks N --pause-ms N --events 0|1] [--seed S --period P --imprint 0|1|2 --n N --box L --grid G --modes M --temp T --gamma G --bind-r R --gain K --window W --rep A --xi X --rcut R --spec S --cool K]");
    std::filesystem::path dir = argv[2]; World w; int pause = 0; uint64_t ticks = 4096; bool config = false, events = true;
    for (int j = 3; j < argc; j += 2) {
        if (j + 1 >= argc) throw std::runtime_error("option sans valeur");
        std::string k = argv[j], v = argv[j + 1];
        if (k == "--ticks") ticks = std::stoull(v); else if (k == "--pause-ms") pause = std::stoi(v);
        else if (k == "--events") events = v != "0";   // journal des projections : exécution seulement, hors checkpoint
        else { config = true;
            if (k == "--seed") w.P.seed = std::stoull(v); else if (k == "--period") w.P.period = std::stoull(v);
            else if (k == "--imprint") w.P.imprint = std::stoull(v); else if (k == "--n") w.P.n = std::stoull(v);
            else if (k == "--box") w.P.box = std::stod(v); else if (k == "--grid") w.P.grid = std::stoull(v);
            else if (k == "--modes") w.P.modes = std::stoull(v); else if (k == "--temp") w.P.temp = std::stod(v);
            else if (k == "--gamma") w.P.gamma = std::stod(v); else if (k == "--bind-r") w.P.bind_r = std::stod(v);
            else if (k == "--gain") w.P.gain = std::stod(v); else if (k == "--window") w.P.window = std::stoull(v);
            else if (k == "--spec") w.P.spec = std::stod(v); else if (k == "--cool") w.P.cool = std::stod(v); else if (k == "--rep") w.P.rep = std::stod(v); else if (k == "--xi") w.P.xi = std::stod(v); else if (k == "--rcut") w.P.rcut = std::stod(v);
            else throw std::runtime_error("option inconnue"); }
    }
    require(w.P.n <= 1024 && w.P.imprint <= 2 && w.P.modes >= 16 && w.P.modes <= 256 && !(w.P.modes & (w.P.modes - 1)) && w.P.grid >= 1 && w.P.grid <= 12, "dimensions invalides");
    std::filesystem::create_directories(dir); auto ck = dir / "state.ck";
    int lockfd = open((dir / "writer.lock").c_str(), O_CREAT | O_RDWR, 0660);
    require(lockfd >= 0 && flock(lockfd, LOCK_EX | LOCK_NB) == 0, "monde deja ouvert ou verrou inaccessible");
    bool resume = std::filesystem::exists(ck);
    if (resume) {
        require(!config, "reprise: ne pas fournir de nouvelle configuration"); w.load(ck);
        recover_log(dir / "events.tsv", w.t); recover_log(dir / "mesures.tsv", w.t); recover_log(dir / "naissances.tsv", w.t);
    } else w.init();
    std::ofstream ev(dir / "events.tsv", std::ios::app), log(dir / "mesures.tsv", std::ios::app), bl(dir / "naissances.tsv", std::ios::app);
    ev.precision(10); log.precision(10); bl.precision(8);
    if (!resume) {
        ev << "t\tsite\tensemble\tvaleur\toctet_ecrit\tphase_ensemble\n"; log << World::header();
        bl << "t\tid\tparent\ttype\tgeneration\ttaille\tphase\tcoherence\tphase_parent\td_parent\td_hasard\n";
        w.metrics(log); w.save(ck);
    }
    uint64_t end = w.t + ticks;
    while (!ticks || w.t < end) {
        w.step(events ? &ev : nullptr, &bl);
        if (w.t % 128 == 0) { w.metrics(log); log.flush(); ev.flush(); bl.flush(); w.save(ck); if (pause) std::this_thread::sleep_for(std::chrono::milliseconds(pause)); }
    }
    if (w.t % 128) w.metrics(log);
    ev.flush(); log.flush(); bl.flush(); w.save(ck); w.metrics(std::cout);
    return 0;
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; } }
