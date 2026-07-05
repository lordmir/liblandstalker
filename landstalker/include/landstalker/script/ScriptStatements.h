#ifndef _SCRIPT_STATEMENTS_H_
#define _SCRIPT_STATEMENTS_H_

#include <string>
#include <vector>
#include <variant>
#include <yaml-cpp/yaml.h>
#include <landstalker/main/AsmFile.h>

namespace Landstalker
{
	class ScriptFunction;
}

namespace Landstalker::Statements
{
	struct Statement
	{
		virtual void ToAsm(AsmFile& file) const = 0;
		// Emits the statement as one item of the enclosing "Statements" block sequence, using
		// yaml-cpp's emitter (which handles indentation, quoting and block scalars) rather than
		// hand-assembled strings. The schema written here is exactly what the statements' YAML
		// constructors read back.
		virtual void ToYaml(YAML::Emitter& out) const = 0;
		virtual std::string Print(int indent = 0) const = 0;
		virtual bool IsEndOfFunction() const = 0;
	};

	struct Action : public Statement
	{
		Action() = default;
		Action(const AsmFile::ScriptAction& scriptaction);
		Action(const ScriptFunction& func);
		Action(AsmFile& file);
		// Parses an action's value node: a mapping holding either "ScriptID", "Jump", or a
		// nested inline function definition ("Function" + "Statements").
		Action(const YAML::Node& node);
		bool operator== (const Action& rhs) const;
		bool operator!= (const Action& rhs) const;
		operator AsmFile::ScriptAction() const;
		virtual void ToAsm(AsmFile& file) const override;
		void ActionToAsm(AsmFile& file, int offset = 0) const;
		virtual void ToYaml(YAML::Emitter& out) const override;
		// Emits the action's value node (a "ScriptID"/"Jump" mapping, or a nested inline
		// function definition) - what every "OnXxx:"-style key stores, and what ActionTable
		// composes into its sequence.
		void ActionToYaml(YAML::Emitter& out) const;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		std::variant<std::monostate, AsmFile::ScriptId, AsmFile::ScriptJump, std::shared_ptr<ScriptFunction>> action;
		std::size_t offset = 0;
	};

	struct YesNoPrompt : public Statement
	{
		YesNoPrompt(AsmFile& file);
		YesNoPrompt(const YAML::Node::const_iterator& it);
		YesNoPrompt(const Action& p_prompt, const Action& p_on_yes, const Action& p_on_no)
			: prompt(p_prompt), on_yes(p_on_yes), on_no(p_on_no) {}

		bool operator== (const YesNoPrompt& rhs) const;
		bool operator!= (const YesNoPrompt& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;

		Action prompt;
		Action on_yes;
		Action on_no;
	};

	struct SetFlagOnTalk : public Statement
	{
		SetFlagOnTalk(AsmFile& file);
		SetFlagOnTalk(const YAML::Node::const_iterator& it);
		SetFlagOnTalk(uint16_t p_flag, const Action& p_on_clear, const Action& p_on_set)
			: flag(p_flag), on_clear(p_on_clear), on_set(p_on_set) {}

		bool operator== (const SetFlagOnTalk& rhs) const;
		bool operator!= (const SetFlagOnTalk& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		uint16_t flag;
		Action on_clear;
		Action on_set;
	};

	struct IsFlagSet : public Statement
	{
		IsFlagSet(AsmFile& file);
		IsFlagSet(const YAML::Node::const_iterator& it);
		IsFlagSet(uint16_t p_flag, const Action& p_on_set, const Action& p_on_clear)
			: flag(p_flag), on_set(p_on_set), on_clear(p_on_clear) {}

		bool operator== (const IsFlagSet& rhs) const;
		bool operator!= (const IsFlagSet& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		uint16_t flag;
		Action on_set;
		Action on_clear;
	};

	struct PlaySound : public Statement
	{
		PlaySound(AsmFile& file);
		PlaySound(const YAML::Node::const_iterator& it);
		explicit PlaySound(uint16_t p_sound) : sound(p_sound) {}

		bool operator== (const PlaySound& rhs) const;
		bool operator!= (const PlaySound& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		uint16_t sound;
	};

	struct CustomItemScript : public Statement
	{
		CustomItemScript(AsmFile& file);
		CustomItemScript(const YAML::Node::const_iterator& it);
		explicit CustomItemScript(uint16_t p_custom_item_script) : custom_item_script(p_custom_item_script) {}

		bool operator== (const CustomItemScript& rhs) const;
		bool operator!= (const CustomItemScript& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		uint16_t custom_item_script;
	};

	struct Sleep : public Statement
	{
		Sleep(AsmFile& file);
		Sleep(const YAML::Node::const_iterator& it);
		explicit Sleep(uint16_t p_ticks) : ticks(p_ticks) {}

		bool operator== (const Sleep& rhs) const;
		bool operator!= (const Sleep& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		uint16_t ticks;
	};

	struct DisplayPrice : public Statement
	{
		DisplayPrice(AsmFile& file);
		DisplayPrice(const YAML::Node::const_iterator& it);
		explicit DisplayPrice(const Action& p_display_price) : display_price(p_display_price) {}

		bool operator== (const DisplayPrice& rhs) const;
		bool operator!= (const DisplayPrice& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		Action display_price;
	};

	struct Branch : public Statement
	{
		Branch(const AsmFile::Instruction& file);
		Branch(const YAML::Node::const_iterator& it);
		Branch(const std::string& p_label, bool p_wide) : label(p_label), wide(p_wide) {}

		bool operator== (const Branch& rhs) const;
		bool operator!= (const Branch& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		std::string label;
		bool wide;
	};

	struct ShopInteraction : public Statement
	{
		ShopInteraction(AsmFile& file);
		ShopInteraction(const YAML::Node::const_iterator& it);
		ShopInteraction(const Action& p_on_sale_prompt, const Action& p_on_sale_confirm, const Action& p_on_no_money, const Action& p_on_sale_decline)
			: on_sale_prompt(p_on_sale_prompt), on_sale_confirm(p_on_sale_confirm), on_no_money(p_on_no_money), on_sale_decline(p_on_sale_decline) {}

		bool operator== (const ShopInteraction& rhs) const;
		bool operator!= (const ShopInteraction& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		Action on_sale_prompt;
		Action on_sale_confirm;
		Action on_no_money;
		Action on_sale_decline;
	};

	struct ChurchInteraction : public Statement
	{
		ChurchInteraction(AsmFile& file);
		ChurchInteraction(const YAML::Node::const_iterator& it);
		ChurchInteraction(const Action& p_script_normal_priest, const Action& p_script_skeleton_priest)
			: script_normal_priest(p_script_normal_priest), script_skeleton_priest(p_script_skeleton_priest) {}

		bool operator== (const ChurchInteraction& rhs) const;
		bool operator!= (const ChurchInteraction& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		Action script_normal_priest;
		Action script_skeleton_priest;
	};

	struct Rts : public Statement
	{
		Rts();
		Rts(const YAML::Node::const_iterator& it);

		bool operator== (const Rts& rhs) const;
		bool operator!= (const Rts& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
	};

	struct CustomAsm : public Statement
	{
		CustomAsm(const AsmFile::Instruction& ins);
		CustomAsm(const YAML::Node::const_iterator& it);
		CustomAsm(const std::string& inst);
		CustomAsm(const std::vector<AsmFile::Instruction>& inst);

		bool operator== (const CustomAsm& rhs) const;
		bool operator!= (const CustomAsm& rhs) const;

		void Append(const AsmFile::Instruction& ins);
		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		std::vector<AsmFile::Instruction> instructions;
	};

	struct ProgressList : public Statement
	{
		ProgressList(AsmFile& file);
		ProgressList(const YAML::Node::const_iterator& it);

		bool operator== (const ProgressList& rhs) const;
		bool operator!= (const ProgressList& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		struct QuestProgress
		{
			QuestProgress(uint8_t p_quest, uint8_t p_progress) : quest(p_quest), progress(p_progress) {}

			QuestProgress() = delete;
			QuestProgress(const QuestProgress&) = default;
			QuestProgress(QuestProgress&&) = default;
			QuestProgress& operator= (const QuestProgress&) = default;
			QuestProgress& operator= (QuestProgress&&) = default;

			bool operator==(const QuestProgress& rhs) const
			{
				return this->quest == rhs.quest && this->progress == rhs.progress;
			}
			bool operator!=(const QuestProgress& rhs) const
			{
				return !(*this == rhs);
			}
			bool operator<(const QuestProgress& rhs) const
			{
				if (this->quest == rhs.quest)
				{
					return this->progress < rhs.progress;
				}
				else
				{
					return this->quest < rhs.quest;
				}
			}

			uint8_t quest;
			uint8_t progress;
		};
		// Declared after QuestProgress so the nested type is visible in the parameter list.
		explicit ProgressList(std::vector<std::pair<QuestProgress, Action>> p_progress) : progress(std::move(p_progress)) {}
		std::vector<std::pair<QuestProgress, Action>> progress;
	};

	struct ProgressFlagMapping : public Statement
	{
		ProgressFlagMapping(AsmFile& file);
		ProgressFlagMapping(const YAML::Node::const_iterator& it);
		ProgressFlagMapping(std::map<uint8_t, uint16_t, std::greater<uint8_t>>&& p_mapping) : mapping(p_mapping) {}

		bool operator== (const ProgressFlagMapping& rhs) const;
		bool operator!= (const ProgressFlagMapping& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		std::map<uint8_t, uint16_t, std::greater<uint8_t>> mapping;
	};

	struct ActionTable : public Statement
	{
		ActionTable(AsmFile& file);
		ActionTable(const YAML::Node::const_iterator& it);
		explicit ActionTable(std::vector<Action> p_actions) : actions(std::move(p_actions)) {}

		bool operator== (const ActionTable& rhs) const;
		bool operator!= (const ActionTable& rhs) const;

		virtual void ToAsm(AsmFile& file) const override;
		virtual void ToYaml(YAML::Emitter& out) const override;
		virtual std::string Print(int indent = 0) const override;
		virtual bool IsEndOfFunction() const override;
		std::vector<Action> actions;
	};

	using ScriptStatement = std::variant
		<
		Statements::Action, Statements::CustomAsm, Statements::ProgressList, Statements::YesNoPrompt, Statements::SetFlagOnTalk,
		Statements::IsFlagSet, Statements::PlaySound, Statements::CustomItemScript, Statements::ActionTable, Statements::Sleep,
		Statements::DisplayPrice, Statements::Branch, Statements::ShopInteraction, Statements::ChurchInteraction,
		Statements::ProgressFlagMapping, Statements::Rts
		>;
	
	using ScriptStatementVector = std::vector<ScriptStatement>;
}

std::ostream& operator<<(std::ostream& lhs, const Landstalker::Statements::Statement& rhs);

#endif // _SCRIPT_STATEMENTS_H_
