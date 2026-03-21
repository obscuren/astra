#!/bin/bash
# 3D depth wall rendering test — run in terminal to preview
# Wall color: bright white (256-color 15)
# Face color: dim gray (256-color 238)
# Floor color: dark gray (256-color 236)
# Fixture: yellow (256-color 3)

W="\033[38;5;15m"   # wall edge (bright)
F="\033[38;5;238m"  # wall face (dim)
FL="\033[38;5;240m" # floor dots
SC="\033[38;5;236m" # floor scatter
FX="\033[38;5;3m"   # fixtures (yellow)
CY="\033[38;5;6m"   # cyan fixtures
GR="\033[38;5;2m"   # green
PL="\033[38;5;11m"  # player yellow
R="\033[0m"         # reset

# Block chars
FULL=$'\xe2\x96\x88'  # █
LTOP=$'\xe2\x96\x84'  # ▄  (lower half = edge at bottom)
FACE=$'\xe2\x96\x80'  # ▀  (upper half = face at top)

echo ""
echo "  === SPACE STATION (3D depth) ==="
echo ""
printf "  ${W}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${R}\n"
printf "  ${F}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${R}\n"
printf "  ${W}${FULL}${FL}. . . . . . .${W}${FULL}${FL}. . . . .${W}${FULL}${R}\n"
printf "  ${W}${FULL}${FL}. . ${FX}${LTOP} ${FL}. . ${FX}${LTOP} ${FL}.${W}${FULL}${FL}. . . . .${W}${FULL}${R}\n"
printf "  ${W}${FULL}${FL}. . . . . . .${W}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${FULL}${R}\n"
printf "  ${W}${FULL}${FL}. . . ${PL}@${FL} . . ${F}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${R}\n"
printf "  ${W}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${FL}. . . . . . .${W}${FULL}${R}\n"
printf "  ${F}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FL}. ${CY}* ${FL}. . . . .${W}${FULL}${R}\n"
printf "  ${W}${FULL}${FL}. . . .${W}${FULL}${FL}. . . . . . .${W}${FULL}${R}\n"
printf "  ${W}${FULL}${FL}. ${GR}+ ${FL}. .${W}${FULL}${FL}. . . . . . .${W}${FULL}${R}\n"
printf "  ${W}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${FULL}${R}\n"

echo ""
echo ""

# Rocky cave version
RW="\033[38;5;250m"  # rocky wall (light gray)
RF="\033[38;5;242m"  # rocky face (medium gray)
RFL="\033[38;5;240m" # rocky floor
RS="\033[38;5;237m"  # rocky scatter

echo "  === ROCKY CAVE (3D depth) ==="
echo ""

B1=$'\xe2\x96\x91'  # ░
B2=$'\xe2\x96\x92'  # ▒

printf "  ${RW}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${R}\n"
printf "  ${RF}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${R}\n"
printf "  ${RW}${B1}${RS} ,  . \` .  ,  . \` ${RW}${B1}${RS}. ,  .${RW}${B2}${R}\n"
printf "  ${RW}${B2}${RS}  . ,  .  \`  . , ${RW}${B1}${RS} \` .  ${RW}${B1}${R}\n"
printf "  ${RW}${B1}${RS} ,  .  .  , \` , ${RW}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${R}\n"
printf "  ${RW}${B2}${RS} . ,  .  \`  . ${RF}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${R}\n"
printf "  ${RW}${B1}${RS}  \`  . ${PL}@${RS} .  , ${RS} . \` .  ${RW}${B1}${R}\n"
printf "  ${RW}${B1}${RS} ,   . \` .  , . \` . ,  ${RW}${B2}${R}\n"
printf "  ${RW}${B2}${RS}  . , \` . ,  . ,  . \` ${RW}${B1}${R}\n"
printf "  ${RW}${B1}${B2}${B1}${B2}${B1}${B1}${B2}${B1}${B2}${B1}${B1}${B2}${B1}${B2}${B1}${B1}${B2}${B1}${B2}${B1}${B1}${B2}${B1}${B2}${B1}${B1}${B2}${R}\n"

echo ""
echo ""

# Volcanic cave
VW="\033[38;5;196m"  # volcanic wall (red)
VF="\033[38;5;52m"   # volcanic face (dark red)
VFL="\033[38;5;52m"  # volcanic floor

B3=$'\xe2\x96\x93'  # ▓
WA=$'\xe2\x89\x88'  # ≈

echo "  === VOLCANIC CAVE (3D depth) ==="
echo ""
printf "  ${VW}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${R}\n"
printf "  ${VF}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${R}\n"
printf "  ${VW}${B3}${VFL} ;  . \` .  ;  . \` ${VW}${B3}${VFL}. ;  .${VW}${B3}${R}\n"
printf "  ${VW}${B3}${VFL}  . ;  .  \`  . ; ${VW}${B3}${VFL} \` .  ${VW}${B3}${R}\n"
printf "  ${VW}${B3}${VFL} ;  . ${VW}${WA}${WA}${WA}${WA}${WA}${VFL}  . \` ${VW}${B3}${VFL}. ;  ${VW}${B3}${R}\n"
printf "  ${VW}${B3}${VFL} . ; ${VW}${WA}${WA}${WA}${WA}${WA}${VFL}  ; . ${VW}${B3}${VFL} \`  .${VW}${B3}${R}\n"
printf "  ${VW}${B3}${VFL}  \`  . ${PL}@${VFL} .  ,  . \` .  ${VW}${B3}${R}\n"
printf "  ${VW}${B3}${VFL} ,   . \` .  ; . \` . ;  ${VW}${B3}${R}\n"
printf "  ${VW}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${B3}${R}\n"
echo ""

# Crystal cave
CW="\033[38;5;201m"  # crystal wall (bright magenta)
CF="\033[38;5;54m"   # crystal face (dark magenta)
CFL="\033[38;5;129m" # crystal floor

DI=$'\xe2\x97\x86'  # ◆
DO=$'\xe2\x97\x87'  # ◇

echo "  === CRYSTAL CAVE (3D depth) ==="
echo ""
printf "  ${CW}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${LTOP}${R}\n"
printf "  ${CF}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${FACE}${R}\n"
printf "  ${CW}${DI}${CFL} .  * . .  .  * . ${CW}${DO}${CFL}*  . .${CW}${DI}${R}\n"
printf "  ${CW}${DO}${CFL}  . .  *  .  . . ${CW}${DI}${CFL} . *  ${CW}${DO}${R}\n"
printf "  ${CW}${DI}${CFL} .  *  .  . * . .${CW}${DO}${CFL} .  . ${CW}${DI}${R}\n"
printf "  ${CW}${DO}${CFL} * .  .  .  . * .${CW}${DI}${CFL} .  * ${CW}${DO}${R}\n"
printf "  ${CW}${DI}${CFL}  .  . ${PL}@${CFL} .  .  * .  .  ${CW}${DI}${R}\n"
printf "  ${CW}${DO}${CFL} .   * . .  . * . . .  ${CW}${DO}${R}\n"
printf "  ${CW}${DI}${DO}${DI}${DO}${DI}${DI}${DO}${DI}${DO}${DI}${DI}${DO}${DI}${DO}${DI}${DI}${DO}${DI}${DO}${DI}${DI}${DO}${DI}${DO}${DI}${DI}${DO}${R}\n"
echo ""
echo ""
