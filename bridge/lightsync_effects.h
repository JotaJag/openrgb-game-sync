#pragma once
// Software equivalents of the SDK's timed lighting commands. All state is
// protected by the adapter's guard; transport still enforces foreground focus.
typedef struct {
    int mode; // 1 flash, 2 pulse
    DWORD start;
    unsigned duration, interval;
    uint32_t from, to, restore;
} TimedEffect;
static TimedEffect global_effect, key_effects[126];
static uint32_t effect_base[126], effect_ambient;
static unsigned char effect_skip[126];
static int effect_target;
static HANDLE animation_stop, animation_thread;
static SRWLOCK animation_lifecycle = SRWLOCK_INIT;

static uint32_t blend_color(uint32_t from, uint32_t to, unsigned factor)
{
    uint32_t result = 0;
    for (unsigned shift=0;shift<24;shift+=8)
        result |= ((((from>>shift)&255)*(255-factor)+((to>>shift)&255)*factor)/255)<<shift;
    return result;
}
static unsigned effect_factor(const TimedEffect* effect, ULONGLONG now)
{
    unsigned phase=(DWORD)(now-effect->start)%effect->interval;
    if(effect->mode==1)return phase*2<effect->interval?255:0;
    return (unsigned)((double)(phase*2<=effect->interval?phase:effect->interval-phase)*510/effect->interval);
}
static int expired(const TimedEffect* effect, ULONGLONG now)
{return effect->duration && (DWORD)(now-effect->start)>=effect->duration;}

static void cancel_global(void)
{
    if(!global_effect.mode)return;
    if(effect_target&4)for(int i=0;i<126;++i)if(!effect_skip[i])keys[i]=effect_base[i];
    if(effect_target&3)ambient=effect_ambient;
    global_effect.mode=0;
}
static void cancel_key(int index)
{
    if(key_effects[index].mode)keys[index]=key_effects[index].restore;
    key_effects[index].mode=0;
}
static int cancel_effects(void)
{
    int changed=global_effect.mode?((effect_target&4?1:0)|(effect_target&3?2:0)):0;
    cancel_global();
    for(int i=0;i<126;++i){if(key_effects[i].mode)changed|=1;cancel_key(i);}
    return changed;
}
static void emit_restored_except(int changed,int replaced)
{
    if((changed&1)&&!(replaced&1))gs_frame(1,6,21,keys);
    if((changed&2)&&!(replaced&2))gs_frame(6,1,1,&ambient);
}
static void animation_tick(ULONGLONG now)
{
    int keyboard_changed=0,ambient_changed=0;
    if(global_effect.mode)
    {
        keyboard_changed=(effect_target&4)!=0;ambient_changed=(effect_target&3)!=0;
        if(expired(&global_effect,now))cancel_global();
        else
        {
            unsigned factor=effect_factor(&global_effect,now);
            if(keyboard_changed)for(int i=0;i<126;++i)if(!effect_skip[i])keys[i]=blend_color(effect_base[i],global_effect.to,factor);
            if(ambient_changed)ambient=blend_color(effect_ambient,global_effect.to,factor);
        }
    }
    for(int i=0;i<126;++i)if(key_effects[i].mode)
    {
        keyboard_changed=1;
        if(expired(&key_effects[i],now))cancel_key(i);
        else keys[i]=blend_color(key_effects[i].from,key_effects[i].to,effect_factor(&key_effects[i],now));
    }
    if(keyboard_changed)gs_frame(1,6,21,keys);
    if(ambient_changed)gs_frame(6,1,1,&ambient);
}
static DWORD WINAPI animate_lighting(void* unused)
{
    (void)unused;
    while(WaitForSingleObject(animation_stop,20)==WAIT_TIMEOUT)
    {
        AcquireSRWLockExclusive(&guard);
        if(initialized)animation_tick(GetTickCount64());
        ReleaseSRWLockExclusive(&guard);
    }
    return 0;
}
static BOOL start_global_effect(int mode,int r,int g,int b,int duration,int interval)
{
    uint32_t color;
    if(!color_value(r,g,b,&color)||duration<0||interval<=0)return FALSE;
    AcquireSRWLockExclusive(&guard);
    int ok=initialized;
    if(ok)
    {
        int cancelled=cancel_effects();memcpy(effect_base,keys,sizeof(keys));effect_ambient=ambient;
        memset(effect_skip,0,sizeof(effect_skip));effect_target=target;
        global_effect=(TimedEffect){mode,GetTickCount64(),(unsigned)duration,(unsigned)interval,0,color,0};
        animation_tick(GetTickCount64());
        emit_restored_except(cancelled,(target&4?1:0)|(target&3?2:0));
    }
    ReleaseSRWLockExclusive(&guard);return ok;
}
