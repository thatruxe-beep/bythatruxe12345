#pragma once

class NewRapid
{
public:
    static void Update();
    // Undo any scaled weapon-table values. Used by Old Rapid Fire before it
    // takes its own snapshot of the original table.
    static void RestoreTable();
};
