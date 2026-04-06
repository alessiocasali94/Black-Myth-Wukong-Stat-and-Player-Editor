#pragma once

#include <string>
#include <vector>

namespace BlackMythWukong
{
    struct TrainerOption
    {
        std::wstring Label;
        std::wstring Hotkey;
        bool Enabled = false;
    };

    struct TrainerSection
    {
        std::wstring Title;
        std::vector<TrainerOption> Options;
    };

    class PseudoTrainerPanel
    {
    public:
        PseudoTrainerPanel();

        [[nodiscard]] const std::vector<TrainerSection>& GetSections() const noexcept;
        [[nodiscard]] bool SaveModsEnabled() const noexcept;
        void ToggleSaveMods() noexcept;
        void ToggleOption(std::size_t sectionIndex, std::size_t optionIndex);

    private:
        std::vector<TrainerSection> m_sections;
        bool m_saveMods = true;
    };
}
