/*
 * @TAG(CUSTOM)
 *
 * The ziggurat method for RNOR and REXP
 *
 * Combine the code below with the main program in which you want
 * normal or exponential variates.
 *
 * Then use of REXP in any expression will provide an exponential variate
 * with density exp(-x), x > 0. Before using REXP in your main, insert a
 * command such as 'zigset(86947731);' with your own choice of seed value > 0,
 * rather than 86947731. If you do not invoke 'zigset(...)' you will get
 * all zeros for REXP.
 *
 * For details of the method, see Marsaglia and Tsang, "The ziggurat method
 * for generating random variables", Journ. Statistical Software.
 *
 * Note: Original code changed to support multiple threads and reduce cache bounce.
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <smp.h>

static struct {
    uint32_t jz;
    uint32_t jsr;
    int32_t  hz;
    uint32_t iz;
    uint32_t kn[128];
    uint32_t ke[256];
    float wn[128];
    float fn[128];
    float we[256];
    float fe[256];
    uint64_t paddingg[8];
} rs[CONFIG_MAX_NUM_NODES] __attribute__((aligned(CACHE_LN_SZ)));;

#define SHR3(id) (rs[id].jz = rs[id].jsr, rs[id].jsr ^= (rs[id].jsr << 13), rs[id].jsr ^= (rs[id].jsr >> 17), rs[id].jsr ^= (rs[id].jsr << 5), rs[id].jz + rs[id].jsr)
#define UNI(id) (0.5 + (int32_t) SHR3(id) * 0.2328306e-9)
#define REXP(id) (rs[id].jz = SHR3(id), rs[id].iz = rs[id].jz & 255, (rs[id].jz < rs[id].ke[rs[id].iz]) ? rs[id].jz * rs[id].we[rs[id].iz] : efix(id))

static float efix(int id)
{
    float x;

    for ( ; ; ) {
        if (rs[id].iz == 0) {
            return (7.69711 - log(UNI(id)));
        }

        x = rs[id].jz * rs[id].we[rs[id].iz];
        if (rs[id].fe[rs[id].iz] + UNI(id) * (rs[id].fe[rs[id].iz - 1] - rs[id].fe[rs[id].iz]) < exp(-x)) {
            return (x);
        }

        rs[id].jz = SHR3(id);
        rs[id].iz = (rs[id].jz & 255);
        if (rs[id].jz < rs[id].ke[rs[id].iz]) {
            return (rs[id].jz * rs[id].we[rs[id].iz]);
        }
    }
}

static void
zigset(int tid, uint32_t jsrseed)
{
    const double m1 = 2147483648.0, m2 = 4294967296.;
    double dn = 3.442619855899, tn = dn, vn = 9.91256303526217e-3, q;
    double de = 7.697117470131487, te = de, ve = 3.949659822581572e-3;
    int i;
    rs[tid].jsr = 123456789;
    rs[tid].jsr ^= jsrseed;

    q = vn / exp(-0.5 * dn * dn);
    rs[tid].kn[0] = (dn / q) * m1;
    rs[tid].kn[1] = 0;

    rs[tid].wn[0] = q / m1;
    rs[tid].wn[127] = dn / m1;

    rs[tid].fn[0] = 1.0;
    rs[tid].fn[127] = exp(-0.5 * dn * dn);

    for (i = 126; i >= 1; i--) {
        dn = sqrt(-2.0 * log(vn / dn + exp(-0.5 * dn * dn)));
        rs[tid].kn[i + 1] = (dn / tn) * m1;
        tn = dn;
        rs[tid].fn[i] = exp(-0.5 * dn * dn);
        rs[tid].wn[i] = dn / m1;
    }

    q = ve / exp(-de);
    rs[tid].ke[0] = (de / q) * m2;
    rs[tid].ke[1] = 0;

    rs[tid].we[0] = q / m2;
    rs[tid].we[255] = de / m2;

    rs[tid].fe[0] = 1.0;
    rs[tid].fe[255] = exp(-de);

    for (i = 254; i >= 1; i--) {
        de = -log(ve / de + exp(-de));
        rs[tid].ke[i + 1] = (de / te) * m2;
        te = de;
        rs[tid].fe[i] = exp(-de);
        rs[tid].we[i] = de / m2;
    }
}
