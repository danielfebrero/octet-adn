// Q1: independent hybrid mean-field experiment; see PROTOCOLE.md.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
using C=std::complex<double>;
constexpr double pi=3.14159265358979323846, box=8, dt=.02, cutoff=2.5;
using V=std::array<double,3>;
V operator+(V a,V b){for(int k=0;k<3;k++)a[k]+=b[k];return a;}
V operator-(V a,V b){for(int k=0;k<3;k++)a[k]-=b[k];return a;}
V operator*(V a,double b){for(auto&x:a)x*=b;return a;}
double norm(V a){return std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);}
V delta(V a,V b){V d=b-a;for(auto&x:d)x-=box*std::round(x/box);return d;}
struct Kernel {double u,du;};
Kernel kernel(double r){
    if(r>=cutoff)return {0,0};
    double e=std::exp(.9-r),u=e*e-2*e,du=2*(e-e*e);
    double z=1-r*r/(cutoff*cutoff),w=z*z,dw=-4*r*z/(cutoff*cutoff);
    return {u*w,du*w+u*dw};
}
// In-place radix-2 transform; inverse carries 1/N, so propagation is unitary.
void fft(std::vector<C>&a,bool inverse){
    size_t n=a.size();
    for(size_t i=1,j=0;i<n;i++){
        size_t b=n>>1;for(;j&b;b>>=1)j^=b;j^=b;
        if(i<j)std::swap(a[i],a[j]);
    }
    for(size_t len=2;len<=n;len<<=1){
        C wlen=std::polar(1.0,(inverse?2:-2)*pi/len);
        for(size_t i=0;i<n;i+=len){C w=1;for(size_t j=0;j<len/2;j++){
            C u=a[i+j],v=a[i+j+len/2]*w;
            a[i+j]=u+v;a[i+j+len/2]=u-v;w*=wlen;
        }}
    }
    if(inverse)for(auto&x:a)x/=n;
}
struct Byte {V x{},v{};uint32_t value=0;};
struct Site {std::vector<C> psi;uint64_t owner=0,streak=0;};
template<class T>void put(std::ostream&o,const T&v){o.write(reinterpret_cast<const char*>(&v),sizeof v);}
template<class T>void get(std::istream&i,T&v){i.read(reinterpret_cast<char*>(&v),sizeof v);if(!i)throw std::runtime_error("checkpoint tronqué");}
struct World {
    uint64_t seed=101,rng=101,t=0,period=16,events=0,maxloop=0,maxassembly=1;
    int modes=256,grid=4;
    std::vector<Byte> bytes;
    std::vector<Site> sites;
    std::vector<uint64_t> ages;
    std::vector<double> cs,sn;
    std::vector<C> freephase;
    size_t links=0,largest=1;
    uint64_t next(){uint64_t z=(rng+=0x9e3779b97f4a7c15ULL);z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;return z^(z>>31);}
    double uniform(){return (next()>>11)*0x1.0p-53;}
    void tables(){
        cs.resize(modes);sn.resize(modes);freephase.resize(modes);
        for(int j=0;j<modes;j++){
            cs[j]=std::cos(2*pi*j/modes);sn[j]=std::sin(2*pi*j/modes);
            int k=j<=modes/2?j:j-modes;
            freephase[j]=std::polar(1.0,-dt*k*k/50.0);
        }
    }
    void init(int n){
        tables();rng=seed;bytes.resize(n);ages.assign(n*n,0);
        for(auto&b:bytes){
            b.value=next()%256;
            for(auto&x:b.x)x=uniform()*box;
            for(auto&v:b.v)v=(uniform()-.5)*.15;
        }
        sites.resize(grid*grid*grid);
        for(auto&s:sites)s.psi.assign(modes,C(1/std::sqrt(double(modes)),0));
    }
    V position(size_t i)const{
        return {(double(i%grid)+.5)*box/grid,(double((i/grid)%grid)+.5)*box/grid,(double(i/(grid*grid))+.5)*box/grid};
    }
    std::array<double,2> moment(const Site&s)const{
        std::array<double,2> m{};
        for(int j=0;j<modes;j++){double p=std::norm(s.psi[j]);m[0]+=p*cs[j];m[1]+=p*sn[j];}return m;
    }
    void evolve(Site&s,double vc,double vs){
        // Constant term of U only adds a global phase; omit it.
        for(int j=0;j<modes;j++)s.psi[j]*=std::polar(1.0,-dt*(vc*cs[j]+vs*sn[j]));
        fft(s.psi,false);for(int j=0;j<modes;j++)s.psi[j]*=freephase[j];fft(s.psi,true);
    }
    int project(Site&s){
        double sum=0;for(auto z:s.psi)sum+=std::norm(z);
        double target=uniform()*sum,acc=0;int chosen=modes-1;
        for(int j=0;j<modes;j++){acc+=std::norm(s.psi[j]);if(target<acc){chosen=j;break;}}
        std::fill(s.psi.begin(),s.psi.end(),C(0));s.psi[chosen]=1;return chosen;
    }
    static double entropy(const Site&s){double h=0;for(C z:s.psi){double p=std::norm(z);if(p>0)h-=p*std::log(p);}return h;}
    void step(std::ostream*ev=nullptr){
        size_t n=bytes.size();std::vector<V> forces(n,V{});
        std::vector<size_t> parent(n);std::iota(parent.begin(),parent.end(),0);
        auto root=[&](size_t a){while(a!=parent[a]){parent[a]=parent[parent[a]];a=parent[a];}return a;};
        links=0;largest=0;
        // Positions are read from one instant; update only after all forces.
        for(size_t i=0;i<n;i++)for(size_t j=i+1;j<n;j++){
            V d=delta(bytes[i].x,bytes[j].x);double r=std::sqrt(norm(d)*norm(d)+.01);
            auto k=kernel(r);double a=1+.5*std::cos(2*pi*(double(bytes[i].value)-bytes[j].value)/256);
            V f=d*(a*k.du/r);forces[i]=forces[i]+f;forces[j]=forces[j]-f;
            uint64_t&age=ages[i*n+j];if(k.du>1e-8)++age;else age=0;
            if(age>=8){++links;size_t u=root(i),v=root(j);if(u!=v)parent[v]=u;}
        }
        std::vector<size_t> sizes(n,0);std::vector<uint64_t> signatures(n,1469598103934665603ULL);
        for(size_t i=0;i<n;i++){size_t r=root(i);++sizes[r];signatures[r]=(signatures[r]^(i+1))*1099511628211ULL;}
        for(auto s:sizes)largest=std::max(largest,s);
        maxassembly=std::max(maxassembly,uint64_t(largest));
        for(size_t si=0;si<sites.size();si++){
            Site&s=sites[si];auto before=moment(s);V p=position(si);
            double vc=0,vs=0;
            std::vector<size_t> hits(n,0);std::vector<double> strengths(n,0);
            std::vector<V> derivatives(n,V{});
            for(size_t i=0;i<n;i++){
                V d=delta(bytes[i].x,p);double r=std::sqrt(norm(d)*norm(d)+.01);auto k=kernel(r);
                double a=2*pi*bytes[i].value/256,co=std::cos(a),sn0=std::sin(a);
                double affinity=1+.5*(co*before[0]+sn0*before[1]);
                forces[i]=forces[i]+d*(affinity*k.du/r);
                vc+=.5*k.u*co;vs+=.5*k.u*sn0;
                derivatives[i]=d*(.5*k.du/r);
                if(std::abs(k.du)>1e-8){size_t r0=root(i);++hits[r0];strengths[r0]+=std::abs(k.du);}
            }
            Site counterfactual=s;
            evolve(counterfactual,0,0);
            evolve(s,vc,vs);auto after=moment(s),without=moment(counterfactual);
            // Measure a causal return: the change in the field's distribution
            // must actually change forces on at least two members of a group.
            std::vector<size_t> returns(n,0);
            for(size_t i=0;i<n;i++){
                double a=2*pi*bytes[i].value/256;
                double da=std::cos(a)*(after[0]-without[0])+std::sin(a)*(after[1]-without[1]);
                if(norm(derivatives[i]*da)>1e-10)++returns[root(i)];
            }
            size_t winner=n;double best=0;
            for(size_t r=0;r<n;r++)if(sizes[r]>=2&&hits[r]>=2&&returns[r]>=2&&strengths[r]>best){winner=r;best=strengths[r];}
            uint64_t owner=winner<n?signatures[winner]:0;
            if(owner&&owner==s.owner)++s.streak;else s.streak=owner?1:0;
            s.owner=owner;maxloop=std::max(maxloop,s.streak);
            if(period&&s.streak>=period){
                double h=entropy(s);int state=project(s);++events;
                if(ev)*ev<<t+1<<'\t'<<si<<'\t'<<owner<<'\t'<<s.streak<<'\t'<<(state*256/modes)<<'\t'<<h<<'\n';
                s.streak=0;
            }
        }
        for(size_t i=0;i<n;i++)for(int k=0;k<3;k++){
            bytes[i].v[k]+=dt*forces[i][k];bytes[i].x[k]+=dt*bytes[i].v[k];
            bytes[i].x[k]-=box*std::floor(bytes[i].x[k]/box);
            if(!std::isfinite(bytes[i].v[k]))throw std::runtime_error("vitesse non finie");
        }
        ++t;
    }
    double normerror()const{
        double e=0;for(auto&s:sites){double z=0;for(auto v:s.psi)z+=std::norm(v);e=std::max(e,std::abs(z-1));}return e;
    }
    double kinetic()const{double k=0;for(auto&b:bytes)for(double v:b.v)k+=.5*v*v;return k;}
    void metrics(std::ostream&o)const{
        o<<t<<'\t'<<events<<'\t'<<links<<'\t'<<largest<<'\t'<<maxassembly<<'\t'<<maxloop<<'\t'<<normerror()<<'\t'<<kinetic()<<'\n';
    }
    void save(const std::filesystem::path&p)const{
        std::ofstream o(p.string()+".tmp",std::ios::binary);o.exceptions(std::ios::badbit|std::ios::failbit);
        uint64_t magic=0x5131574f524c4401ULL;put(o,magic);put(o,seed);put(o,rng);put(o,t);put(o,period);put(o,events);put(o,maxloop);put(o,maxassembly);put(o,modes);put(o,grid);put(o,links);put(o,largest);
        uint64_t n=bytes.size();put(o,n);
        for(auto&b:bytes){for(double x:b.x)put(o,x);for(double v:b.v)put(o,v);put(o,b.value);}
        for(auto a:ages)put(o,a);
        for(auto&s:sites){put(o,s.owner);put(o,s.streak);for(auto z:s.psi){double re=z.real(),im=z.imag();put(o,re);put(o,im);}}
        o.close();std::filesystem::rename(p.string()+".tmp",p);
    }
    void load(const std::filesystem::path&p){
        std::ifstream i(p,std::ios::binary);uint64_t magic;get(i,magic);
        if(magic!=0x5131574f524c4401ULL)throw std::runtime_error("version checkpoint incorrecte");
        get(i,seed);get(i,rng);get(i,t);get(i,period);get(i,events);get(i,maxloop);get(i,maxassembly);get(i,modes);get(i,grid);get(i,links);get(i,largest);
        uint64_t n;get(i,n);if(n>1024||grid<1||grid>8||modes<16||modes>256||(modes&(modes-1)))throw std::runtime_error("dimensions checkpoint invalides");
        tables();bytes.resize(n);ages.resize(n*n);sites.resize(grid*grid*grid);
        for(auto&b:bytes){for(double&x:b.x)get(i,x);for(double&v:b.v)get(i,v);get(i,b.value);}
        for(auto&a:ages)get(i,a);
        for(auto&s:sites){get(i,s.owner);get(i,s.streak);s.psi.resize(modes);for(auto&z:s.psi){double re,im;get(i,re);get(i,im);z={re,im};}}
        if(normerror()>1e-7)throw std::runtime_error("norme checkpoint invalide");
    }
};
void require(bool p,const char*msg){if(!p)throw std::runtime_error(msg);}
void recover_log(const std::filesystem::path&p,uint64_t tick){
    if(!std::filesystem::exists(p))throw std::runtime_error("journal manquant pour reprise");
    std::ifstream in(p);std::ofstream out(p.string()+".recover");
    out.exceptions(std::ios::badbit|std::ios::failbit);
    std::string line;bool header=true;
    while(std::getline(in,line)){
        if(header){out<<line<<'\n';header=false;continue;}
        // A crash may leave a partial line after the last atomic checkpoint.
        auto tab=line.find('\t');if(tab==std::string::npos)continue;
        try{if(std::stoull(line.substr(0,tab))<=tick)out<<line<<'\n';}catch(const std::exception&){continue;}
    }
    out.close();in.close();std::filesystem::rename(p.string()+".recover",p);
}
void selftest(){
    require(kernel(.3).du<0&&kernel(1.5).du>0,"signes force");
    require(kernel(cutoff).u==0&&kernel(cutoff).du==0,"coupure");
    for(double r:{.3,.9,1.5,2.49})require(std::abs((kernel(r+1e-6).u-kernel(r-1e-6).u)/2e-6-kernel(r).du)<1e-7,"gradient");
    V a{1,2,3},b{2,2,3};V d=delta(a,b);auto k=kernel(std::sqrt(1.01));V f=d*k.du,g=delta(b,a)*k.du;require(norm(f+g)<1e-14,"reciprocite paire");
    World w;w.init(0);for(int j=0;j<100;j++)w.step();require(w.events==0&&w.normerror()<1e-10,"vide");
    World lone;lone.init(1);lone.period=1;for(int j=0;j<100;j++)lone.step();require(lone.events==0,"pas ensemble");
    World u;u.init(2);Site s=u.sites[0];for(int j=0;j<100;j++)u.evolve(s,1,.5);
    double z=0;for(C x:s.psi)z+=std::norm(x);require(std::abs(z-1)<1e-10,"unitarite");
    s.psi.assign(u.modes,C{});s.psi[37]=1;require(u.project(s)==37,"projection Born support");
    int count=0;for(int j=0;j<10000;j++){s.psi.assign(u.modes,C{});s.psi[0]=std::sqrt(.25);s.psi[1]=std::sqrt(.75);count+=u.project(s)==0;}require(count>2300&&count<2700,"projection Born frequences");
    // Two octets at attractive separation, no hand-made bond: wait 8 ticks.
    World pair;pair.init(2);pair.period=1;pair.bytes[0].x={3,3,3};pair.bytes[1].x={4.4,3,3};pair.bytes[0].v={};pair.bytes[1].v={};
    for(int j=0;j<7;j++)pair.step();
    require(pair.events==0,"pas de lien premature");
    for(int j=0;j<20;j++)pair.step();
    require(pair.events>0,"boucle puis projection");
    World ctrl=pair;ctrl.period=0;uint64_t old=ctrl.events;for(int j=0;j<30;j++)ctrl.step();require(ctrl.events==old,"temoin sans projection");
    std::cout<<"selftest: gradient, signes, coupure, paire reciproque, vide, singleton, unitarite, Born, boucle et temoin OK\n";
}
int main(int argc,char**argv){try{
    if(argc==2&&std::string(argv[1])=="selftest"){selftest();return 0;}
    if(argc<3||std::string(argv[1])!="run")throw std::runtime_error("usage: monde run DIR [--ticks N --seed S --period P --n N --modes M --pause-ms N]");
    std::filesystem::path dir=argv[2];World w;int n=64,pause=0;uint64_t ticks=4096;
    bool config=false;
    for(int j=3;j<argc;j+=2){if(j+1>=argc)throw std::runtime_error("option sans valeur");std::string k=argv[j];uint64_t v=std::stoull(argv[j+1]);
        if(k=="--ticks")ticks=v;else if(k=="--pause-ms")pause=v;
        else{config=true;if(k=="--seed")w.seed=v;else if(k=="--period")w.period=v;else if(k=="--n")n=v;else if(k=="--modes")w.modes=v;else throw std::runtime_error("option inconnue");}
    }
    require(n>=0&&n<=1024&&w.modes>=16&&w.modes<=256&&!(w.modes&(w.modes-1)),"dimensions invalides");
    std::filesystem::create_directories(dir);auto ck=dir/"state.ck";
    int lockfd=open((dir/"writer.lock").c_str(),O_CREAT|O_RDWR,0660);
    require(lockfd>=0&&flock(lockfd,LOCK_EX|LOCK_NB)==0,"monde deja ouvert ou verrou inaccessible");
    bool resume=std::filesystem::exists(ck);
    if(resume){
        require(!config,"reprise: ne pas fournir de nouvelle configuration");w.load(ck);
        recover_log(dir/"events.tsv",w.t);recover_log(dir/"mesures.tsv",w.t);
    }else w.init(n);
    std::ofstream ev(dir/"events.tsv",std::ios::app),log(dir/"mesures.tsv",std::ios::app);
    ev.precision(17);log.precision(17);
    if(!resume){ev<<"t\tsite\tassembly\tloop\tbyte\tentropy_before\n";log<<"t\tprojections\tliens\tplus_grand\tmaximum_assemblage\tmaximum_boucle\terreur_norme\tenergie_cinetique\n";w.metrics(log);w.save(ck);}
    uint64_t end=w.t+ticks;
    while(!ticks||w.t<end){
        w.step(&ev);
        if(w.t%128==0){w.metrics(log);log.flush();ev.flush();w.save(ck);if(pause)std::this_thread::sleep_for(std::chrono::milliseconds(pause));}
    }
    if(w.t%128)w.metrics(log);
    ev.flush();log.flush();w.save(ck);w.metrics(std::cout);
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
