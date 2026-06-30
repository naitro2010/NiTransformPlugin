scriptName NiTransformPlugin Hidden
bool Function SetNiTransformFrequency(Actor a_actor, Armor a_armor, string n,float freq) global native
bool Function SetNiTransformStart(Actor a_actor, Armor a_armor, string n) global native
bool Function SetNiTransformStop(Actor a_actor, Armor a_armor, string n) global native
bool Function SetNiTransformStopDelayed(Actor a_actor, Armor a_armor, string n) global native
bool Function SetNiTransformPause(Actor a_actor, Armor a_armor, string n, bool pause) global native
Function TestStartStop(Actor target,Armor a_armor,string n, bool started) global
        if started
                SetNiTransformStart(target,a_armor,n)    
        Else
                SetNiTransformStopDelayed(target,a_armor,n)    
        EndIf
EndFunction
Function TestPause(Actor target,Armor a_armor,string n, bool started) global    
        SetNiTransformPause(target,a_armor,n,!started)    
EndFunction
Function TestFreq(Actor target,Armor a_armor,string n, float freq) global    
        if (freq > 0.0)
                SetNiTransformFrequency(target,a_armor,n,freq)    
        EndIf
EndFunction