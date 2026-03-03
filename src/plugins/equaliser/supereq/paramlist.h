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

#ifndef __PARAMLIST_H__
#define __PARAMLIST_H__

class paramlistelm
{
public:
    paramlistelm* next{nullptr};

    char left;
    char right;
    float lower;
    float upper;
    float gain;
    float gain2{0};
    int sortindex{0};

    paramlistelm()
    {
        left = right = 1;
        lower = upper = gain = 0;
    };

    ~paramlistelm()
    {
        delete next;
        next = nullptr;
    };

    /*	char *getString(void) {
            static char str[64];
            sprintf(str,"%gHz to %gHz, %gdB %c%c",
                (double)lower,(double)upper,(double)gain,left?'L':' ',right?'R':' ');
            return str;
        }*/
};

class paramlist
{
public:
    paramlistelm* elm{nullptr};

    ~paramlist()
    {
        delete elm;
        elm = nullptr;
    }

    void copy(paramlist& src)
    {
        delete elm;
        elm = nullptr;

        paramlistelm** p;
        paramlistelm* q;

        for(p = &elm, q = src.elm; q != nullptr; q = q->next, p = &(*p)->next) {
            *p          = new paramlistelm;
            (*p)->left  = q->left;
            (*p)->right = q->right;
            (*p)->lower = q->lower;
            (*p)->upper = q->upper;
            (*p)->gain  = q->gain;
        }
    }

    paramlistelm* newelm()
    {
        paramlistelm** e;
        for(e = &elm; *e != nullptr; e = &(*e)->next) {
            ;
        }

        *e = new paramlistelm;

        return *e;
    }

    int getnelm()
    {
        int i;
        paramlistelm* e;

        for(e = elm, i = 0; e != nullptr; e = e->next, i++) {
            ;
        }

        return i;
    }

    void delelm(paramlistelm* p)
    {
        paramlistelm** e;

        for(e = &elm; *e != nullptr && p != *e; e = &(*e)->next) {
            ;
        }

        if(*e == nullptr) {
            return;
        }

        *e      = (*e)->next;
        p->next = nullptr;
        delete p;
    }

    void sortelm()
    {
        int i{0};

        if(elm == nullptr) {
            return;
        }

        for(paramlistelm* r = elm; r != nullptr; r = r->next) {
            r->sortindex = i++;
        }

        paramlistelm** p;
        paramlistelm** q;

        for(p = &elm->next; *p != nullptr;) {
            for(q = &elm; *q != *p; q = &(*q)->next) {
                if((*p)->lower < (*q)->lower || ((*p)->lower == (*q)->lower && (*p)->sortindex < (*q)->sortindex)) {
                    break;
                }
            }

            if(p == q) {
                p = &(*p)->next;
                continue;
            }

            paramlistelm** pn = p;
            paramlistelm* pp  = *p;
            *p                = (*p)->next;
            pp->next          = *q;
            *q                = pp;

            p = pn;
        }
    }
};

#endif // __PARAMLIST_H__
