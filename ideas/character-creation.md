# Feature: Character Creation & Stats

In order to have a proper character we need to make changes. This features requires a "standard" fully developed character (much like we have now) for when we select the "developer" mode new game. We'll work on a proper "new game" later.

## Character

This is what a character data should contain:

* Name
* Level
* Experience
* Class
* Race
* Attributes
    * Primary:
        - Strength (STR)
        - Agility (AGI)
        - Touchness (TOU)
        - Intelligence (INT)
        - Willpower (WIL)
        - Luck (LUC)
    * Secondary
        - Quickness (QN)
        - Move Speed (MS)
        - Armor Value (AV)
        - Dodge Value (DV)
    * Resistance
        - Acid (AR)
        - Electrical (ER)
        - Cold (CR)
        - Heat (HR)
* Skill (This will be an array of skills the user can have. They may consist of passive and active skill)
    = Acrobatics:
        * Swiftness "You get +5 bonus to DV when attacked with missile weapons"
    = Short Blade
        * (Passive) Short Blade Expertise "You get +1 hit with short blades. Also, the action cost of attacking with a short blade in your primary hand is reduced with 25%"
        * (Active) Jab "Immediatly jab with your off hand short blade"
    = Thinkering
        * TODO
    = Pistol
        * TODO
    ++++ Many more to come ++++
* Equipment
* Inventory
* Quests (missions)
* Repuration (Array of rep)
* Journal (Knowledge about waypoints, monsters, etc.)


## UI

We'll also need a near full screen UI (fixed width/height) do display all this new information (See screenshots). I want a similar UI.

## Next

After this we'll be implemented the character creation.
