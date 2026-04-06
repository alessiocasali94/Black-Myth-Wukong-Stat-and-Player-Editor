#include "BlackMythWukongPseudoTrainerPanel.h"

#include <stdexcept>

namespace BlackMythWukong
{
    PseudoTrainerPanel::PseudoTrainerPanel()
        : m_sections{
            { L"Player",
                {
                    { L"God Mode/Ignore Hits", L"Numpad 1", false },
                    { L"Infinite Health", L"Numpad 2", true },
                    { L"Infinite Mana", L"Numpad 3", true },
                    { L"Infinite Stamina", L"Numpad 4", true },
                    { L"Infinite Focus", L"Numpad 5", true },
                    { L"Unlimited Gourd Uses", L"Numpad 6", true },
                    { L"Instant Spell Cooldown", L"Numpad 7", true },
                    { L"Instant Transform Cooldown", L"Numpad 8", true },
                    { L"Infinite Transform Duration", L"Numpad 9", true },
                    { L"Infinite Spirit Energy", L"Numpad 0", true },
                    { L"Infinite Vessel Energy", L"Numpad .", true },
                } },
            { L"Combat",
                {
                    { L"Max Critical Chance", L"Numpad +", false },
                    { L"Max Poise", L"Numpad -", false },
                    { L"Prevent Negative Status Effects", L"Numpad /", false },
                    { L"Super Damage/One-Hit Kills", L"Ctrl+Numpad 1", false },
                    { L"Damage Multiplier", L"Ctrl+Numpad 2", false },
                    { L"Defense Multiplier", L"Ctrl+Numpad 3", false },
                    { L"Stamina Consumption Rate", L"Ctrl+Numpad 4", false },
                    { L"Set Game Speed", L"Ctrl+Numpad 5", false },
                } },
            { L"Progression",
                {
                    { L"Will Multiplier", L"Ctrl+Numpad /", false },
                    { L"Experience Multiplier", L"Ctrl+Numpad *", false },
                    { L"Edit Relic Points", L"Ctrl+Num +", false },
                    { L"100% Item Drop Rate", L"Ctrl+Num -", false },
                    { L"Edit Will", L"Alt+Numpad 1", false },
                    { L"Edit Selected Item Amount", L"Alt+Numpad 2", false },
                    { L"Increase Experience", L"Alt+Numpad 3", false },
                    { L"Edit Sparks", L"Alt+Numpad 4", false },
                } },
            { L"Movement",
                {
                    { L"Set Player Speed", L"Alt+Numpad 5", false },
                    { L"Set Movement Speed", L"Alt+Numpad 6", false },
                    { L"Set Jump Height", L"Alt+Numpad 7", false },
                } },
            { L"Attributes",
                {
                    { L"Edit Max Health", L"Shift+F1", false },
                    { L"Edit Max Mana", L"Shift+F2", false },
                    { L"Edit Max Stamina", L"Shift+F3", false },
                    { L"Edit Attack", L"Shift+F4", false },
                    { L"Edit Defense", L"Shift+F5", false },
                    { L"Edit Stamina Recovery Speed", L"Shift+F6", false },
                    { L"Edit Critical Chance", L"Shift+F7", false },
                    { L"Increase Critical Damage", L"Shift+F8", false },
                    { L"Edit Damage Bonus", L"Shift+F9", false },
                    { L"Edit Damage Reduction", L"Shift+F10", false },
                } },
            { L"Resistances",
                {
                    { L"Edit Chill Resistance", L"Ctrl+F1", false },
                    { L"Edit Burn Resistance", L"Ctrl+F2", false },
                    { L"Edit Poison Resistance", L"Ctrl+F3", false },
                    { L"Edit Shock Resistance", L"Ctrl+F4", false },
                } }
        }
    {
    }

    const std::vector<TrainerSection>& PseudoTrainerPanel::GetSections() const noexcept
    {
        return m_sections;
    }

    bool PseudoTrainerPanel::SaveModsEnabled() const noexcept
    {
        return m_saveMods;
    }

    void PseudoTrainerPanel::ToggleSaveMods() noexcept
    {
        m_saveMods = !m_saveMods;
    }

    void PseudoTrainerPanel::ToggleOption(std::size_t sectionIndex, std::size_t optionIndex)
    {
        if (sectionIndex >= m_sections.size() || optionIndex >= m_sections[sectionIndex].Options.size())
        {
            throw std::out_of_range("Pseudo trainer option index is out of range.");
        }

        TrainerOption& option = m_sections[sectionIndex].Options[optionIndex];
        option.Enabled = !option.Enabled;
    }
}
