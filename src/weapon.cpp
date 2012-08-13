// weapon.cpp: all shooting and effects code

#include "cube.h"

struct guninfo { short sound, attackdelay, damage, projspeed, part, kickamount; const char *name; };

const int MONSTERDAMAGEFACTOR = 4;
const int SGRAYS = 20;
const float SGSPREAD = 2;
vec sg[SGRAYS];

static guninfo guns[NUMGUNS] =
{
    { S_PUNCH1,    250,  50, 0,   0,  1, "fist"           },
    { S_SG,       1400,  10, 0,   0, 20, "shotgun"        },  // *SGRAYS
    { S_CG,        100,  30, 0,   0,  7, "chaingun"       },
    { S_RLFIRE,    800, 120, 80,  0, 10, "rocketlauncher" },
    { S_RIFLE,    1500, 100, 0,   0, 30, "rifle"          },
    { S_FLAUNCH,   200,  20, 50,  4,  1, "fireball"       },
    { S_ICEBALL,   200,  40, 30,  6,  1, "iceball"        },
    { S_SLIMEBALL, 200,  30, 160, 7,  1, "slimeball"      },
    { S_PIGR1,     250,  50, 0,   0,  1, "bite"           },
};

void selectgun(int a, int b, int c)
{
    if(a<-1 || b<-1 || c<-1 || a>=NUMGUNS || b>=NUMGUNS || c>=NUMGUNS) return;
    int s = player1->gunselect;
    if(a>=0 && s!=a && player1->ammo[a]) s = a;
    else if(b>=0 && s!=b && player1->ammo[b]) s = b;
    else if(c>=0 && s!=c && player1->ammo[c]) s = c;
    else if(s!=GUN_RL && player1->ammo[GUN_RL]) s = GUN_RL;
    else if(s!=GUN_CG && player1->ammo[GUN_CG]) s = GUN_CG;
    else if(s!=GUN_SG && player1->ammo[GUN_SG]) s = GUN_SG;
    else if(s!=GUN_RIFLE && player1->ammo[GUN_RIFLE]) s = GUN_RIFLE;
    else s = GUN_FIST;
    if(s!=player1->gunselect) sound::playc(S_WEAPLOAD);
    player1->gunselect = s;
    //console::out("%s selected", (int)guns[s].name);
};

int reloadtime(int gun) { return guns[gun].attackdelay; };

void weapon(char *a1, char *a2, char *a3)
{
    selectgun(a1[0] ? atoi(a1) : -1,
              a2[0] ? atoi(a2) : -1,
              a3[0] ? atoi(a3) : -1);
};

COMMAND(weapon, ARG_3STR);

void createrays(vec &from, vec &to)             // create random spread of rays for the shotgun
{
    vdist(dist, dvec, from, to);
    float f = dist*SGSPREAD/1000;
    loopi(SGRAYS)
    {
        #define RNDD (rnd(101)-50)*f
        vec r = { RNDD, RNDD, RNDD };
        sg[i] = to;
        vadd(sg[i], r); 
    };
};

bool intersect(dynent *d, vec &from, vec &to)   // if lineseg hits entity bounding box
{
    vec v = to, w = d->o, *p; 
    vsub(v, from);
    vsub(w, from);
    float c1 = dotprod(w, v);

    if(c1<=0) p = &from;
    else
    {
        float c2 = dotprod(v, v);
        if(c2<=c1) p = &to;
        else
        {
            float f = c1/c2;
            vmul(v, f);
            vadd(v, from);
            p = &v;
        };
    };

    return p->x <= d->o.x+d->radius
        && p->x >= d->o.x-d->radius
        && p->y <= d->o.y+d->radius
        && p->y >= d->o.y-d->radius
        && p->z <= d->o.z+d->aboveeye
        && p->z >= d->o.z-d->eyeheight;
};

char *playerincrosshair()
{
	if(demoplayback) return NULL;
    loopv(players)
    {
        dynent *o = players[i];
        if(!o) continue; 
        if(intersect(o, player1->o, worldpos)) return o->name;
    };
    return NULL;
};

const int MAXPROJ = 100;
struct projectile { vec o, to; float speed; dynent *owner; int gun; bool inuse, local; };
projectile projs[MAXPROJ];

void projreset() { loopi(MAXPROJ) projs[i].inuse = false; };

void newprojectile(vec &from, vec &to, float speed, bool local, dynent *owner, int gun)
{
    loopi(MAXPROJ)
    {
        projectile *p = &projs[i];
        if(p->inuse) continue;
        p->inuse = true;
        p->o = from;
        p->to = to;
        p->speed = speed;
        p->local = local;
        p->owner = owner;
        p->gun = gun;
        return;
    };
};

void hit(int target, int damage, dynent *d, dynent *at)
{
    if(d==player1) selfdamage(damage, at==player1 ? -1 : -2, at);
    else if(d->monsterstate) monsterpain(d, damage, at);
    else { addmsg(1, 4, SV_DAMAGE, target, damage, d->lifesequence); sound::play(S_PAIN1+rnd(5), &d->o); };
    particle_splash(3, damage, 1000, d->o);
	demodamage(damage, d->o);
};

const float RL_RADIUS = 5;
const float RL_DAMRAD = 7;   // hack

void radialeffect(dynent *o, vec &v, int cn, int qdam, dynent *at)
{
    if(o->state!=CS_ALIVE) return;
    vdist(dist, temp, v, o->o);
    dist -= 2; // account for eye distance imprecision
    if(dist<RL_DAMRAD) 
    {
        if(dist<0) dist = 0;
        int damage = (int)(qdam*(1-(dist/RL_DAMRAD)));
        hit(cn, damage, o, at);
        vmul(temp, (RL_DAMRAD-dist)*damage/800);
        vadd(o->vel, temp);
    };
};

void splash(projectile *p, vec &v, vec &vold, int notthisplayer, int notthismonster, int qdam)
{
    particle_splash(0, 50, 300, v);
    p->inuse = false;
    if(p->gun!=GUN_RL)
    {
        sound::play(S_FEXPLODE, &v);
        // no push?
    }
    else
    {
        sound::play(S_RLHIT, &v);
        newsphere(v, RL_RADIUS, 0);
        dodynlight(vold, v, 0, 0, p->owner);
        if(!p->local) return;
        radialeffect(player1, v, -1, qdam, p->owner);
        loopv(players)
        {
            if(i==notthisplayer) continue;
            dynent *o = players[i];
            if(!o) continue; 
            radialeffect(o, v, i, qdam, p->owner);
        };
        dvector &mv = getmonsters();
        loopv(mv) if(i!=notthismonster) radialeffect(mv[i], v, i, qdam, p->owner);
    };
};

inline void projdamage(dynent *o, projectile *p, vec &v, int i, int im, int qdam)
{
    if(o->state!=CS_ALIVE) return;
    if(intersect(o, p->o, v))
    {
        splash(p, v, p->o, i, im, qdam);
        hit(i, qdam, o, p->owner);
    }; 
};

void moveprojectiles(float time)
{
    loopi(MAXPROJ)
    {
        projectile *p = &projs[i];
        if(!p->inuse) continue;
        int qdam = guns[p->gun].damage*(p->owner->quadmillis ? 4 : 1);
        if(p->owner->monsterstate) qdam /= MONSTERDAMAGEFACTOR;
        vdist(dist, v, p->o, p->to);
        float dtime = dist*1000/p->speed;
        if(time>dtime) dtime = time;
        vmul(v, time/dtime);
        vadd(v, p->o)
        if(p->local)
        {
            loopv(players)
            {
                dynent *o = players[i];
                if(!o) continue; 
                projdamage(o, p, v, i, -1, qdam);
            };
            if(p->owner!=player1) projdamage(player1, p, v, -1, -1, qdam);
            dvector &mv = getmonsters();
            loopv(mv) if(!vreject(mv[i]->o, v, 10.0f) && mv[i]!=p->owner) projdamage(mv[i], p, v, -1, i, qdam);
        };
        if(p->inuse)
        {
            if(time==dtime) splash(p, v, p->o, -1, -1, qdam);
            else
            {
                if(p->gun==GUN_RL) { dodynlight(p->o, v, 0, 255, p->owner); particle_splash(5, 2, 200, v); }
                else { particle_splash(1, 1, 200, v); particle_splash(guns[p->gun].part, 1, 1, v); };
            };       
        };
        p->o = v;
    };
};

void shootv(int gun, vec &from, vec &to, dynent *d, bool local)     // create visual effect from a shot
{
    sound::play(guns[gun].sound, d==player1 ? NULL : &d->o);
    int pspeed = 25;
    switch(gun)
    {
        case GUN_FIST:
            break;

        case GUN_SG:
        {
            loopi(SGRAYS) particle_splash(0, 5, 200, sg[i]);
            break;
        };

        case GUN_CG:
            particle_splash(0, 100, 250, to);
            //particle_trail(1, 10, from, to);
            break;

        case GUN_RL:
        case GUN_FIREBALL:
        case GUN_ICEBALL:
        case GUN_SLIMEBALL:
            pspeed = guns[gun].projspeed;
            if(d->monsterstate) pspeed /= 2;
            newprojectile(from, to, (float)pspeed, local, d, gun);
            break;

        case GUN_RIFLE: 
            particle_splash(0, 50, 200, to);
            particle_trail(1, 500, from, to);
            break;
    };
};

void hitpush(int target, int damage, dynent *d, dynent *at, vec &from, vec &to)
{
    hit(target, damage, d, at);
    vdist(dist, v, from, to);
    vmul(v, damage/dist/50);
    vadd(d->vel, v);
};

void raydamage(dynent *o, vec &from, vec &to, dynent *d, int i)
{
    if(o->state!=CS_ALIVE) return;
    int qdam = guns[d->gunselect].damage;
    if(d->quadmillis) qdam *= 4;
    if(d->monsterstate) qdam /= MONSTERDAMAGEFACTOR;
    if(d->gunselect==GUN_SG)
    {
        int damage = 0;
        loop(r, SGRAYS) if(intersect(o, from, sg[r])) damage += qdam;
        if(damage) hitpush(i, damage, o, d, from, to);
    }
    else if(intersect(o, from, to)) hitpush(i, qdam, o, d, from, to);
};

void shoot(dynent *d, vec &targ)
{
    int attacktime = lastmillis-d->lastaction;
    if(attacktime<d->gunwait) return;
    d->gunwait = 0;
    if(!d->attacking) return;
    d->lastaction = lastmillis;
    d->lastattackgun = d->gunselect;
    if(!d->ammo[d->gunselect]) { sound::playc(S_NOAMMO); d->gunwait = 250; d->lastattackgun = -1; return; };
    if(d->gunselect) d->ammo[d->gunselect]--;
    vec from = d->o;
    vec to = targ;
    from.z -= 0.2f;    // below eye
    
    vdist(dist, unitv, from, to);
    vdiv(unitv, dist);
    vec kickback = unitv;
    vmul(kickback, guns[d->gunselect].kickamount*-0.01f);
    vadd(d->vel, kickback);
    if(d->pitch<80.0f) d->pitch += guns[d->gunselect].kickamount*0.05f;
    

    if(d->gunselect==GUN_FIST || d->gunselect==GUN_BITE) 
    {
        vmul(unitv, 3); // punch range
        to = from;
        vadd(to, unitv);
    };   
    if(d->gunselect==GUN_SG) createrays(from, to);

    if(d->quadmillis && attacktime>200) sound::playc(S_ITEMPUP);
    shootv(d->gunselect, from, to, d, true);
    if(!d->monsterstate) addmsg(1, 8, SV_SHOT, d->gunselect, (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF), (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF));
    d->gunwait = guns[d->gunselect].attackdelay;

    if(guns[d->gunselect].projspeed) return;
    
    loopv(players)
    {
        dynent *o = players[i];
        if(!o) continue; 
        raydamage(o, from, to, d, i);
    };

    dvector &v = getmonsters();
    loopv(v) if(v[i]!=d) raydamage(v[i], from, to, d, -2);

    if(d->monsterstate) raydamage(player1, from, to, d, -1);
};

