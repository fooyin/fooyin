/******************************************************
SuperEQ written by Naoki Shibata  shibatch@users.sourceforge.net

Shibatch Super Equalizer is a graphic and parametric equalizer plugin
for winamp. This plugin uses 16383th order FIR filter with FFT algorithm.
It's equalization is very precise. Equalization setting can be done
for each channel separately.

Homepage : http://shibatch.sourceforge.net/
e-mail   : shibatch@users.sourceforge.net

Some changes are from foobar2000 (www.foobar2000.org):

Copyright (c) 2001-2003, Peter Pawlowski
All rights reserved.

Other changes are:

Copyright © 2026, Luke Taylor <luket@pm.me>
*******************************************************/

#ifndef _SUPEREQ_H_
#define _SUPEREQ_H_

#include "fft.h"
#include "paramlist.h"
#include "supereq_compat.h"

#include <cmath>
#include <cstring>

#ifndef NOVTABLE
#if defined(_MSC_VER)
#define NOVTABLE __declspec(novtable)
#else
#define NOVTABLE
#endif
#endif

class NOVTABLE supereq_base
{
public:
    virtual void equ_makeTable(double* bc, class paramlist* param, double fs) = 0;
    virtual void equ_clearbuf()                                               = 0;

    virtual void write_samples(audio_sample* buf, int nsamples) = 0;
    virtual audio_sample* get_output(int* nsamples)             = 0;
    virtual int samples_buffered()                              = 0;
    virtual ~supereq_base()                                     = default;
};

template <class REAL>
class supereq : public supereq_base
{
public:
    enum
    {
        NBANDS = 17
    };

private:
    enum
    {
        M = 15
    };

    pfc::array_t<int> m_rfft_ip;
    pfc::array_t<REAL> m_rfft_w;

    REAL fact[M + 1];
    REAL aa{96};
    REAL iza{0};

    pfc::array_t<REAL> lires;
    pfc::array_t<REAL> lires1;
    pfc::array_t<REAL> lires2;
    pfc::array_t<REAL> irest;
    pfc::array_t<REAL> fsamples;

    volatile int chg_ires{0};
    volatile int cur_ires{0};
    int winlen{0};
    int winlenbit{0};
    int tabsize{0};
    int nbufsamples{0};
    int firstframe{1};

    pfc::array_t<REAL> inbuf, outbuf;
    pfc::array_t<audio_sample> done;
    int samples_done{0};

    void rfft(int n, int isign, REAL x[])
    {
        if(n == 0) {
            m_rfft_ip.set_size(0);
            m_rfft_ip.set_size(0);
            return;
        }

        const t_size newipsize = 2 + std::sqrt(static_cast<float>(n / 2));
        if(newipsize > m_rfft_ip.get_size()) {
            m_rfft_ip.set_size(newipsize);
            m_rfft_ip[0] = 0;
        }

        t_size newwsize = n / 2;
        if(newwsize > m_rfft_ip.get_size()) {
            m_rfft_w.set_size(newwsize);
        }

        fft<REAL>::rdft(n, isign, x, m_rfft_ip.get_ptr(), m_rfft_w.get_ptr());
    }

    REAL izero(REAL x)
    {
        REAL ret{1};

        for(int m{1}; m <= M; ++m) {
            REAL t;
            t = pow(x / 2, m) / fact[m];
            ret += t * t;
        }

        return ret;
    }

    REAL win(REAL n, int N)
    {
        return izero(alpha(aa) * sqrt(1 - (4 * n * n / ((N - 1) * (N - 1))))) / iza;
    }

    void equ_init(int wb)
    {
        lires1.set_size(0);
        lires2.set_size(0);
        irest.set_size(0);
        fsamples.set_size(0);

        winlen    = (1 << (wb - 1)) - 1;
        winlenbit = wb;
        tabsize   = 1 << wb;

        lires1.set_size(tabsize);
        lires1.fill_null();
        lires2.set_size(tabsize);
        lires2.fill_null();
        irest.set_size(tabsize);
        irest.fill_null();
        fsamples.set_size(tabsize);
        fsamples.fill_null();
        inbuf.set_size(winlen);
        inbuf.fill_null();
        outbuf.set_size(tabsize);
        outbuf.fill_null();

        lires    = lires1;
        cur_ires = 1;
        chg_ires = 1;

        for(int i = 0; i <= M; ++i) {
            fact[i] = 1;
            for(int j = 1; j <= i; ++j) {
                fact[i] *= j;
            }
        }

        iza = izero(alpha(aa));
    }

    static REAL alpha(REAL a)
    {
        if(a <= 21) {
            return 0;
        }
        if(a <= 50) {
            return (0.5842 * pow((a - 21.), 0.4)) + (0.07886 * (a - 21));
        }
        return 0.1102 * (a - 8.7);
    }

    static REAL sinc(REAL x)
    {
        return x == 0 ? 1 : sin(x) / x;
    }

    static REAL hn_lpf(int n, REAL f, REAL fs)
    {
        static constexpr auto Pi = static_cast<REAL>(3.14159265358979323846264338327950288L);
        REAL t                   = 1 / fs;
        REAL omega               = 2 * Pi * f;
        return 2 * f * t * sinc(n * omega * t);
    }

    static REAL hn_imp(int n)
    {
        return n == 0 ? 1.0 : 0.0;
    }

    static REAL hn(int n, const paramlist& param2, REAL fs)
    {
        paramlistelm* e;
        REAL ret;
        REAL lhn;

        lhn = hn_lpf(n, param2.elm->upper, fs);
        ret = param2.elm->gain * lhn;

        for(e = param2.elm->next; e->next != nullptr && e->upper < fs / 2; e = e->next) {
            REAL lhn2 = hn_lpf(n, e->upper, fs);
            ret += e->gain * (lhn2 - lhn);
            lhn = lhn2;
        }

        ret += e->gain * (hn_imp(n) - lhn);

        return ret;
    }

    static void process_param(const double* bc, paramlist* param, paramlist& param2, double fs, int ch)
    {
        static constexpr double bands[]
            = {65.406392, 92.498606, 130.81278, 184.99721, 261.62557, 369.99442, 523.25113, 739.9884, 1046.5023,
               1479.9768, 2093.0045, 2959.9536, 4186.0091, 5919.9072, 8372.0181, 11839.814, 16744.036};

        paramlistelm** pp;
        paramlistelm* p;
        paramlistelm* e2;

        int i;

        delete param2.elm;
        param2.elm = nullptr;

        for(i = 0, pp = &param2.elm; i <= NBANDS; ++i, pp = &(*pp)->next) {
            (*pp)        = new paramlistelm;
            (*pp)->lower = i == 0 ? 0 : bands[i - 1];
            (*pp)->upper = i == NBANDS ? fs : bands[i];
            (*pp)->gain  = bc[i];
        }

        for(paramlistelm* e = param->elm; e != nullptr; e = e->next) {
            if((ch == 0 && !e->left) || (ch == 1 && !e->right)) {
                continue;
            }
            if(e->lower >= e->upper) {
                continue;
            }

            for(p = param2.elm; p != nullptr; p = p->next) {
                if(p->upper > e->lower) {
                    break;
                }
            }

            while(p != nullptr && p->lower < e->upper) {
                if(e->lower <= p->lower && p->upper <= e->upper) {
                    p->gain *= pow(10.0, e->gain / 20.0);
                    p = p->next;
                    continue;
                }
                if(p->lower < e->lower && e->upper < p->upper) {
                    e2        = new paramlistelm;
                    e2->lower = e->upper;
                    e2->upper = p->upper;
                    e2->gain  = p->gain;
                    e2->next  = p->next;
                    p->next   = e2;

                    e2        = new paramlistelm;
                    e2->lower = e->lower;
                    e2->upper = e->upper;
                    e2->gain  = p->gain * pow(10., e->gain / 20.0);
                    e2->next  = p->next;
                    p->next   = e2;

                    p->upper = e->lower;

                    p = p->next->next->next;
                    continue;
                }
                if(p->lower < e->lower) {
                    e2        = new paramlistelm;
                    e2->lower = e->lower;
                    e2->upper = p->upper;
                    e2->gain  = p->gain * pow(10.0, e->gain / 20.0);
                    e2->next  = p->next;
                    p->next   = e2;

                    p->upper = e->lower;
                    p        = p->next->next;
                    continue;
                }
                if(e->upper < p->upper) {
                    e2        = new paramlistelm;
                    e2->lower = e->upper;
                    e2->upper = p->upper;
                    e2->gain  = p->gain;
                    e2->next  = p->next;
                    p->next   = e2;

                    p->upper = e->upper;
                    p->gain  = p->gain * pow(10.0, e->gain / 20.0);
                    p        = p->next->next;
                    continue;
                }
                abort();
            }
        }
    }

public:
    explicit supereq(int wb = 14)
    {
        memset(fact, 0, sizeof(fact));

        equ_init(wb);
    }

    void equ_makeTable(double* bc, class paramlist* param, double fs) override
    {
        int i;
        int cires = cur_ires;

        if(fs <= 0) {
            return;
        }

        paramlist param2;

        // L

        process_param(bc, param, param2, fs, 0);

        for(i = 0; i < winlen; i++) {
            irest[i] = hn(i - (winlen / 2), param2, fs) * win(i - (winlen / 2), winlen);
        }

        for(; i < tabsize; i++) {
            irest[i] = 0;
        }

        rfft(tabsize, 1, irest.get_ptr());

        REAL* nires = cires == 1 ? lires2.get_ptr() : lires1.get_ptr();

        for(i = 0; i < tabsize; i++) {
            nires[i] = irest[i];
        }

        chg_ires = cires == 1 ? 2 : 1;
    }

    void equ_clearbuf() override
    {
        firstframe   = 1;
        samples_done = 0;
        nbufsamples  = 0;
    }

    void write_samples(audio_sample* buf, int nsamples) override
    {
        int i;

        if(chg_ires) {
            cur_ires = chg_ires;
            lires    = cur_ires == 1 ? lires1 : lires2;
            chg_ires = 0;
        }

        int p = 0;

        int flush_length = 0;

        if(!buf) // flush
        {
            if(nbufsamples == 0) {
                return;
            }
            flush_length = nbufsamples;
            nsamples     = winlen - nbufsamples;
        }

        while(nbufsamples + nsamples >= winlen) {
            if(buf) {
                for(i = 0; i < winlen - nbufsamples; ++i) {
                    inbuf[nbufsamples + i] = static_cast<REAL>(buf[i + p]);
                }
            }
            else {
                for(i = 0; i < winlen - nbufsamples; ++i) {
                    inbuf[nbufsamples + i] = 0;
                }
            }

            for(i = winlen; i < tabsize; ++i) {
                outbuf[i - winlen] = outbuf[i];
            }

            p += winlen - nbufsamples;
            nsamples -= winlen - nbufsamples;
            nbufsamples = 0;

            REAL* ires = lires.get_ptr();
            for(i = 0; i < winlen; ++i) {
                fsamples[i] = inbuf[i];
            }

            for(i = winlen; i < tabsize; ++i) {
                fsamples[i] = 0;
            }

            rfft(tabsize, 1, fsamples.get_ptr());

            fsamples[0] = ires[0] * fsamples[0];
            fsamples[1] = ires[1] * fsamples[1];

            for(i = 1; i < tabsize / 2; ++i) {
                REAL re;
                REAL im;
                re = (ires[i * 2] * fsamples[i * 2]) - (ires[(i * 2) + 1] * fsamples[(i * 2) + 1]);
                im = (ires[(i * 2) + 1] * fsamples[i * 2]) + (ires[i * 2] * fsamples[(i * 2) + 1]);

                fsamples[i * 2]       = re;
                fsamples[(i * 2) + 1] = im;
            }
            rfft(tabsize, -1, fsamples.get_ptr());

            for(i = 0; i < winlen; ++i) {
                outbuf[i] += fsamples[i] / tabsize * 2;
            }

            for(i = winlen; i < tabsize; ++i) {
                outbuf[i] = fsamples[i] / tabsize * 2;
            }

            int out_length = flush_length > 0 ? flush_length + (winlen / 2) : winlen;

            done.grow_size(samples_done + out_length);

            for(i = firstframe ? winlen / 2 : 0; i < out_length; i++) {
                done[samples_done++] = outbuf[i];
            }
            firstframe = 0;
        }

        if(buf) {
            for(i = 0; i < nsamples; ++i) {
                inbuf[nbufsamples + i] = static_cast<REAL>(buf[i + p]);
            }
            p += nsamples;
        }
        nbufsamples += nsamples;
    }

    audio_sample* get_output(int* nsamples) override
    {
        *nsamples    = samples_done;
        samples_done = 0;
        return done.get_ptr();
    }

    int samples_buffered() override
    {
        return nbufsamples;
    }

    ~supereq() override = default;
};

#endif
