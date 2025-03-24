# MIDI Implementation

## Channel Voice Messages

|MIDI Message|HEX Code|Description|Compatibility|
|:---------- |:------ |:--------- |:-----------:|
|NOTE ON|9nH kk vv|Midi channel n(0-15) note ON #kk(0-127), velocity vv(1-127).<br>vv=0 means NOTE OFF|MIDI|
|NOTE OFF|8nH kk vv|Midi channel n(0-15) note OFF #kk(0-127), vv is ignored.|MIDI|
|PITCH BEND|EnH bl bh|Pitch bend as specified by bh\|bl (14 bits).<br>Maximum swing is +/- 1 tone (power-up).<br>Can be changed using « Pitch bend sensitivity ».<br>Center position is 00H 40H.|GM|
|PROGRAM<br>CHANGE|CnH pp|Program (patch) change.<br>Specific action on channel 10 (n=9) : select drumset.|GM/GS|
|CHANNEL<br>AFTERTOUCH|DnH vv|vv pressure value|MIDI|
|CTRL 01|BnH 01H cc|Modulation wheel|MIDI|
|CTRL 06|BnH 06H cc|Data entry MSB : provides data to RPN and NRPN|MIDI|
|CTRL 07|BnH 07H cc|Volume|MIDI|
|CTRL 10|BnH 0AH cc|Pan|MIDI|
|CTRL 11|BnH 0BH cc|Expression|MIDI/GM|
|CTRL 38|BnH 26H cc|Data entry LSB : provides data to RPN and NRPN|MIDI|
|CTRL 64|BnH 40H cc|Sustain (damper) pedal|MIDI|
|CTRL 66|BnH 42H cc|Sostenuto pedal|MIDI|
|CTRL 67|BnH 43H cc|Soft pedal|MIDI|
|CTRL 98|BnH 62H vv|NRPN LSB|MIDI|
|CTRL 99|BnH 63H vv|NRPN MSB|MIDI|
|CTRL 100|BnH 64H vv|RPN LSB|MIDI|
|CTRL 101|BnH 65H vv|RPN MSB|MIDI|
|CTRL 120|BnH 78H xx|All sounds off (abrupt stop of sound on channel n)|MIDI|
|CTRL 121|BnH 79H xx|Reset all controllers|MIDI|
|CTRL 123|BnH 7BH xx|All notes off|MIDI|
|RPN 0000H|BnH 65H 00H 64H 00H 26H vl 06H vh|Pitch bend sensitivity in semitones #vh(0-12)|MIDI/GM|
|RPN 0001H|BnH 65H 00H 64H 01H 26H vl 06H vh|Fine tuning in cents (vh,vl=00 00H -100, vh,vl=40 00H 0, vh,vl=7F 7FH +99.99)|MIDI|
|RPN 0002H|BnH 65H 00H 64H 02H 06H vv|Coarse tuning in half-tones (vv=28H -24, vv=40H 0, vv=58H +24)|MIDI|

## System Exclusive Messages

|Manufacturer ID|HEX Code|Description|Compatibility|
|:------------- |:------ |:--------- |:-----------:|
|Universal Non-Real Time|F0H 7EH 7FH 09H 01H F7H|General MIDI reset|GM|
|Roland Corporation|F0H 41H 10H 42H 12H 40H 00H 7FH 00H 41H F7H|GS reset<br>The same processing will be carried out as when « GM reset » is received.|GS|
|Casio Computer Co.,Ltd.|F0H 44H 0EH 03H 1vH F7H|Set polyphony (v:0 = 24; v:1 = 32; v:2 = 48; v:3 = 64)||
|Casio Computer Co.,Ltd.|F0H 44H 0EH 03H 2vH F7H|Set reverb (v:0 = Reverb OFF; v:1 = Reverb 1; v:2 = Reverb 2)||
|Casio Computer Co.,Ltd.|F0H 44H 0EH 03H 4vH F7H|Set effect - values=0-10 (v:0-A)||

