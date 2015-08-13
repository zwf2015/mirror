#include "fight_fight.h"
#include <QFile>
#include <QMessageBox>
#include <QThread>
#include <time.h>
#include <QKeyEvent>

#include "Item_Base.h"
#include "def_System_para.h"
#include "mirrorlog.h"
#include "CommonComponents.h"

//定义并初始化静态数据成员。
bool fight_fight::bCheckHp = false;
bool fight_fight::bCheckMp = false;
bool fight_fight::bCheckConcise = false;
bool fight_fight::bCheckFindBoss = false;
qint32 fight_fight::FilterAdd = 0;
qint32 fight_fight::FilterLvl = 0;
qint32 fight_fight::limit_rhp = 50;
qint32 fight_fight::limit_rmp = 50;

extern RoleInfo_False g_falseRole;
extern roleAddition g_roleAddition;

extern vecBuff g_buffList;
extern QVector<Info_jobAdd> g_JobAddSet;
extern QMap<skillID, Info_skill> g_skillList;
extern QMap<itemID, Info_Item> g_ItemList;
extern QMap<mapID, Info_Distribute> g_MonsterDistribute;
extern QVector<MonsterInfo> g_MonsterNormal_List;
extern QVector<MonsterInfo> g_MonsterBoss_list;
extern mapDrop	g_mapDropSet;

fight_fight::fight_fight(QWidget* parent, qint32 id, RoleInfo *info, MapRoleSkill *skill, MapItem *bag_item, ListEquip *bag_equip)
	: QDialog(parent), m_MainFrame(parent), m_mapID(id), myRole(info), m_skill(skill), m_bag_item(bag_item), m_bag_equip(bag_equip)
{
	ui.setupUi(this);
	InitUI();

	Cacl_Display_Role_basic_info();
	CalcRoleInfo();
	LoadItem();

	AssignMonster(g_MonsterNormal_List, g_MonsterBoss_list, g_MonsterDistribute);
	monster_cur = &g_MonsterNormal_List[monster_normal_assign[0]];
	Display_CurrentMonsterInfo();

	bFighting = false;
	bCheckHp = bCheckMp = true;
	nShowStatusRound = 0;
	nSkillIndex = 0;
	m_dlg_fightInfo = nullptr;

	if (m_mapID > 2000) {
		nTimeOutTime = 999 * 60 * 1000;
	} else {
		nTimeOutTime = 5 * 60 * 1000;
	}

	nFightTimer = startTimer(nFightInterval);
	ct_start = QDateTime::currentDateTime();
	nCount_normalMonster = nCount_boss = nCount_exp = nCount_coin = nCount_rep = 0;
	nElapse_pre_boss = 0;
	nCount_fail = nCount_timeout = 0;

	nXSpeedTimer = startTimer(nXSpeedInvterval);
	xSpeedTime.start();
	nXSpeedCount = 0;

	connect(ui.filter_add, SIGNAL(currentIndexChanged(int)), this, SLOT(pickFilterChange(int)));
	connect(ui.filter_level, SIGNAL(textChanged(const QString &)), this, SLOT(on_filter_level_textEdited(const QString &)));
}

fight_fight::~fight_fight()
{

}

void fight_fight::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape)
	{
		on_btn_quit_clicked();
	}
}
void fight_fight::on_btn_quit_clicked(void)
{
	limit_rhp = ui.edit_hp->text().toInt();
	limit_rmp = ui.edit_mp->text().toInt();
	close();
}

void fight_fight::on_btn_statistics_clicked(void)
{
	if (m_dlg_fightInfo == nullptr)
	{
		m_dlg_fightInfo = new fight_info(this);
	}
	QPoint pos = QPoint(730, 370);// mapFromGlobal(cursor().pos()) + QPoint(20, 0);
	m_dlg_fightInfo->move(pos);

	qint32 time = ct_start.secsTo(QDateTime::currentDateTime()) / 60;
	m_dlg_fightInfo->updateInfo(time, nCount_fail, nCount_timeout, nCount_normalMonster, nCount_boss, nCount_exp, nCount_coin, nCount_rep);
	m_dlg_fightInfo->show();
}
void fight_fight::on_filter_level_textEdited(const QString & text)
{
	if (!text.isEmpty())
	{
		FilterLvl = text.toUInt();	
	}
	else
	{
		FilterLvl = 0;
		ui.filter_level->setText(QString::number(FilterLvl));
	}
}
void fight_fight::pickFilterChange(int index)
{
	FilterAdd = index * 2 - 1;
	if (FilterAdd < 0)
	{
		FilterAdd = 0;
	}
}

void fight_fight::InitUI()
{
	bCheckAuto = false;

	ui.progressBar_monster_hp->setStyleSheet("QProgressBar::chunk { background-color: rgb(255, 0, 0) }");
	ui.progressBar_monster_mp->setStyleSheet("QProgressBar::chunk { background-color: rgb(0, 0, 255) }");
	ui.edit_monster_sc->setText("0 - 0");
	ui.edit_monster_rmp->setText("0");

	ui.edit_hp->setText(QString::number(limit_rhp));
	ui.edit_mp->setText(QString::number(limit_rmp));
 	ui.checkBox_concise->setChecked(bCheckConcise);
	ui.checkBox_boss->setChecked(bCheckFindBoss);
	ui.filter_add->setCurrentIndex((FilterAdd + 1) / 2);
	ui.filter_level->setText(QString::number(FilterLvl));

	buffDisp_Role[0] = ui.lbl_role_buff_0;
	buffDisp_Role[1] = ui.lbl_role_buff_1;
	buffDisp_Role[2] = ui.lbl_role_buff_2;
	buffDisp_Mon[0] = ui.lbl_monster_buff_0;
	buffDisp_Mon[1] = ui.lbl_monster_buff_1;
	buffDisp_Mon[2] = ui.lbl_monster_buff_2;

	for (qint32 i = 0; i < MaxBuffCount; i++)
	{
		buffDisp_Role[i]->setText("");
		buffDisp_Mon[i]->setText("");
	}
}

void fight_fight::Cacl_Display_Role_basic_info()
{
	ui.edit_role_name->setText(myRole->name);

	QString VocImg = QString(":/mirror/Resources/ui/f_0_%1.png").arg(myRole->vocation + 1);
	ui.lbl_role_vocation->setPixmap(QPixmap(VocImg));

	qint32 headNo = ((myRole->vocation - 1) * 2 + myRole->gender);
	QString headImg = QString(":/mirror/Resources/head/%1.png").arg(headNo);
	ui.label_role_head->setPixmap(QPixmap(headImg));

	//从整个技能列表中单独提取出挂机技能，以节约后续调用的效率	
	for (auto iterRole = m_skill->constBegin(); iterRole != m_skill->constEnd(); iterRole++)
	{
		if (iterRole->Used && g_skillList.value(iterRole->id).times == 0)
		{
			fightingSkill.append(g_skillList.value(iterRole->id));
		}
	}
	for (auto iterRole = m_skill->constBegin(); iterRole != m_skill->constEnd(); iterRole++)
	{
		if (iterRole->Used && g_skillList.value(iterRole->id).times != 0)
		{
			fightingSkill.append(g_skillList.value(iterRole->id));
		}
	}
}

const Info_Item* fight_fight::FindItem(itemID id)
{
	if (g_ItemList.contains(id))
		return &g_ItemList[id];
	else
		return nullptr;
}
const Info_Item* fight_fight::FindItem(const QString &name)
{
	foreach(const Info_Item &item, g_ItemList)
	{
		if (item.name == name)
		{
			return &item;
		}
	}
	return nullptr;
}

void fight_fight::LoadItem()
{
	QString strTmp;
	for (MapItem::iterator iter = m_bag_item->begin(); iter != m_bag_item->end(); iter++)
	{
		const Info_Item *itemItem = FindItem(iter.key());
		if (itemItem != nullptr && itemItem->level <= Role_Lvl)
		{
			if (itemItem->type == et_immediate_hp)
			{
				strTmp = Generate_ItemComboBox_Text(itemItem->name, QStringLiteral("血"), itemItem->value, iter.value());
				ui.comboBox_hp->addItem(strTmp);
			}
			else if (itemItem->type == et_immediate_mp)
			{
				strTmp = Generate_ItemComboBox_Text(itemItem->name, QStringLiteral("魔"), itemItem->value, iter.value());;
				ui.comboBox_mp->addItem(strTmp);
			}
		}
	}
	ui.comboBox_hp->setCurrentIndex(ui.comboBox_hp->count() - 1);
	ui.comboBox_mp->setCurrentIndex(ui.comboBox_mp->count() - 1);
}

bool fight_fight::AssignMonster(QVector<MonsterInfo> normalList, QVector<MonsterInfo> bossList, QMap<mapID, Info_Distribute> Distribute)
{
	quint32 c;
	memset(monster_normal_assign, 0, Max_monster * sizeof(quint32));
	memset(monster_boss_assign, 0, Max_monster * sizeof(quint32));

	const Info_Distribute &dis = Distribute[m_mapID];

	c = 0;
	foreach(quint32 n, dis.normal)
	{
		monster_normal_assign[c++] = n;
	}
	monster_normal_count = c;

	c = 0;
	foreach(quint32 n, dis.boss)
	{
		monster_boss_assign[c++] = n;
	}
	if (monster_boss_assign[0] == 0)
	{
		monster_boss_count = 0;			//有些地图不刷新BOSS
	}
	else
	{
		monster_boss_count = c;
	}

	//将怪物ID转化为其在总怪物列表中的索引序号，以方便后续加载。
	c = 0;
	for (quint32 i = 0; i < normalList.size() && c < monster_normal_count; i++)
	{
		if (monster_normal_assign[c] == normalList[i].ID)
		{
			monster_normal_assign[c++] = i;
		}
	}
	c = 0;
	for (quint32 i = 0; i < bossList.size() && c < monster_boss_count; i++)
	{
		if (monster_boss_assign[c] == bossList[i].ID)
		{
			monster_boss_assign[c++] = i;
		}
	}
	return true;
}

void fight_fight::Display_CurrentMonsterInfo()
{
	//设置体、魔最大值。
	ui.progressBar_monster_hp->setMaximum(monster_cur->hp);
	ui.progressBar_monster_mp->setMaximum(monster_cur->mp);
	//显示当前体、魔
	monster_cur_hp = monster_cur->hp;
	ui.progressBar_monster_hp->setValue(monster_cur_hp);
	ui.progressBar_monster_mp->setValue(monster_cur->mp);
	
	//只有普通地图的怪有回血功能。
	if (m_mapID < 2000) {	
		monster_cur_rhp = monster_cur->hp >> 7;
	} else {
		monster_cur_rhp = 0;
	}		
	ui.edit_monster_rhp->setText(QString::number(monster_cur_rhp));

	//加载头像
	ui.label_monster_head->setPixmap(QPixmap::fromImage(monster_cur->Head));

	//加载其他属性
	monster_cur_ac = monster_cur->AC;
	monster_cur_mac = monster_cur->MAC;
	ui.edit_monster_name->setText(monster_cur->name);
	ui.edit_monster_level->setText(QStringLiteral("Lv:") + QString::number(monster_cur->level));
	ui.edit_monster_dc->setText(QString::number(monster_cur->DC1) + " - " + QString::number(monster_cur->DC2));
	ui.edit_monster_mc->setText(QString::number(monster_cur->MC1) + " - " + QString::number(monster_cur->MC2));
	ui.edit_monster_ac->setText(QString::number(monster_cur_ac));
	ui.edit_monster_mac->setText(QString::number(monster_cur_mac));
	ui.edit_monster_interval->setText(QString::number(monster_cur->interval));
}

inline QString fight_fight::Generate_ItemComboBox_Text(const QString &name, const QString &type, quint32 value, quint32 count)
{
	QString strSplit = QStringLiteral("%1 %2:%3 剩:%4").arg(name).arg(type).arg(value).arg(count);
	return strSplit;
}
inline QString fight_fight::Generate_Display_LineText(const QString &str1, const QString &skill, const QString &str2, bool bLuck, bool bep, QList<qint32> listDamage)
{
	QString strTmp = QStringLiteral("<font color=DarkCyan>%1</font>使用<font color=gray>%2</font>，").arg(str1).arg(skill);
	if (bLuck)
		strTmp += QStringLiteral("获得战神祝福,");

	strTmp += QStringLiteral("对<font color = DarkCyan>%1</font>").arg(str2);
	
	if (bep)
		strTmp += QStringLiteral("造成<font color = red>致命</font>伤害:<font color = magenta>");
	else
		strTmp += QStringLiteral("造成伤害:<font color = magenta>");

	if (listDamage.size() == 0)
	{
		strTmp += "0";
	}
	for (qint32 i = 0; i < listDamage.size(); i++)
	{
		strTmp += QString::number(listDamage.at(i)) + " ";
	}

	strTmp += QStringLiteral("</font>");
	return strTmp;
}

void fight_fight::Step_role_UsingItem_hp(void)
{
	quint32 ID, nTmp1;

	QString strTmp = ui.comboBox_hp->currentText();
	QStringList strList = strTmp.split(" ");

	const Info_Item *itemItem = FindItem(strList.at(0));
	if (itemItem != nullptr)
	{
		ID = itemItem->ID;
		//背包对应道具数量减1
		m_bag_item->insert(ID, m_bag_item->value(ID) - 1); 
		strTmp = Generate_ItemComboBox_Text(itemItem->name, QStringLiteral("血"), itemItem->value, m_bag_item->value(ID));
		ui.comboBox_hp->setItemText(ui.comboBox_hp->currentIndex(), strTmp);

		//更改角色状态
		nTmp1 = FourCharToInt(myRole->hp_1, myRole->hp_2, myRole->hp_3, myRole->hp_4);
		role_hp_c += itemItem->value;
		if (role_hp_c >= nTmp1)
		{
			role_hp_c = nTmp1;
		}
		ui.progressBar_role_hp->setValue(role_hp_c);
		if (!bCheckConcise)
		{
			strTmp = QStringLiteral("<font color=green>你使用了：") + itemItem->name + QStringLiteral("</font>");
			ui.edit_display->append(strTmp);
		}

		//如果道具已经用完，则删除当前道具.如果还有道具，则切换到0号道具，否则清除自动补血复选。
		if (m_bag_item->value(ID) <= 0)
		{
			m_bag_item->remove(ID);
			ui.comboBox_hp->removeItem(ui.comboBox_hp->currentIndex());
			if (ui.comboBox_hp->count() > 0)
			{
				ui.comboBox_hp->setCurrentIndex(0);
			}
			else
			{
				ui.checkBox_hp->setChecked(false);
				bCheckHp = false;
			}
		}
	}
	else
	{	//找不到对应道具，清除自动补血复选。
		ui.checkBox_hp->setCheckState(Qt::Unchecked);
		bCheckHp = false;
	}
}
void fight_fight::Step_role_UsingItem_mp(void)
{
	quint32 ID, nTmp1;
	QString strTmp = ui.comboBox_mp->currentText();
	QStringList strList = strTmp.split(" ");

	const Info_Item *itemItem = FindItem(strList.at(0));
	if (itemItem != NULL)
	{
		ID = itemItem->ID;
		//背包对应道具数量减1
		m_bag_item->insert(ID, m_bag_item->value(ID) - 1);
		strTmp = Generate_ItemComboBox_Text(itemItem->name, QStringLiteral("魔"), itemItem->value, m_bag_item->value(ID));
		ui.comboBox_mp->setItemText(ui.comboBox_mp->currentIndex(), strTmp);

		//更改角色状态
		nTmp1 = FourCharToInt(myRole->mp_1, myRole->mp_2, myRole->mp_3, myRole->mp_4);
		role_mp_c += itemItem->value;
		if (role_mp_c >= nTmp1)
		{
			role_mp_c = nTmp1;
		}
		ui.progressBar_role_mp->setValue(role_mp_c);
		if (!bCheckConcise)
		{
			strTmp = QStringLiteral("<font color=green>你使用了：") + itemItem->name + QStringLiteral("</font>");
			ui.edit_display->append(strTmp);
		}
		//如果道具已经用完，则删除当前道具.如果还有道具，则切换到0号道具，否则清除自动补血复选。
		if (m_bag_item->value(ID) <= 0)
		{
			m_bag_item->remove(ID);
			ui.comboBox_mp->removeItem(ui.comboBox_mp->currentIndex());
			if (ui.comboBox_mp->count() > 0)
			{
				ui.comboBox_mp->setCurrentIndex(0);
			}
			else
			{
				ui.checkBox_mp->setChecked(false);
				bCheckMp = false;
			}
		}
	}
	else
	{	//找不到对应道具，清除复选。
		ui.checkBox_mp->setCheckState(Qt::Unchecked);
		bCheckMp = false;
	}
}

inline quint32 fight_fight::GetRoleATK(qint32 type, bool &bLuck)
{
	quint32 nA, Min, Max, nTmp3;
	double dTmp;

	Min = 0;
	Max = 1;
	if (type == 1)
	{
		Min = FourCharToInt(myRole->dc1_1, myRole->dc2_1, myRole->dc1_3, myRole->dc1_4);
		Max = FourCharToInt(myRole->dc2_1, myRole->dc2_2, myRole->dc2_3, myRole->dc2_4);
	}
	else if (type == 2)
	{
		Min = FourCharToInt(myRole->mc1_1, myRole->mc1_2, myRole->mc1_3, myRole->mc1_4);
		Max = FourCharToInt(myRole->mc2_1, myRole->mc2_2, myRole->mc2_3, myRole->mc2_4);
	}
	else if (type == 3)
	{
		Min = FourCharToInt(myRole->sc1_1, myRole->sc1_2, myRole->sc1_3, myRole->sc1_4);
		Max = FourCharToInt(myRole->sc2_1, myRole->sc2_2, myRole->sc2_3, myRole->sc2_4);
	}

	nA = Min + qrand() % (Max - Min + 1);

	//发挥幸运
	dTmp = 20.0 * qrand() / RAND_MAX;
	nTmp3 = myRole->luck_1 << 8 | myRole->luck_2;
	if (dTmp < nTmp3)
	{
		nA = Max;
		bLuck = true;
	}
	return nA;
}

void fight_fight::Step_role_Skill(void)
{
	bool bUsedSkill = false;
	qint32 spell, nTmp;

	for (qint32 i = 0; i < fightingSkill.size(); i++)
	{
		nTmp = nSkillIndex;
		const skill_fight &skill = fightingSkill.at(nSkillIndex++);
		if (nSkillIndex >= fightingSkill.size())
		{
			nSkillIndex = 0;
		}

		spell = skill.spell;
		if (role_mp_c < spell)
		{
			QString strTmp = QStringLiteral("<font color=red>魔法不足，无法施放技能.</font>");
			ui.edit_display->append(strTmp);
			return;
		}	

		if (skill.cd_c <= 0)
		{
			if (skill.buff > 0)
			{
				if (m_mapID > 1000 && m_mapID < 2000 && skill.buff > 100)
				{
					QString strTmp = QStringLiteral("<font color=red>怪物拥有魔神庇佑.%1无效</font>").arg(skill.name);
					ui.edit_display->append(strTmp);
					bUsedSkill = true;
				}
				else
				{
					bUsedSkill = MStep_role_Buff(skill);
				}			
			}

			if (skill.times > 0)
			{
				bUsedSkill = MStep_role_Attack(skill);
			}
		}
		if (bUsedSkill)
		{
			fightingSkill[nTmp].cd_c = fightingSkill[nTmp].cd;
			role_mp_c -= spell;
			ui.progressBar_role_mp->setValue(role_mp_c);
			break;
		}
	}
	if (!bUsedSkill)
	{
		ui.edit_display->append(QStringLiteral("无可用技能"));
	}
}
bool fight_fight::MStep_role_Buff(const skill_fight &skill)
{
	quint32 nTmp, nTmp1;
	bool bLuck = false;
	if (skill.buff > 100)
	{
		foreach(const realBuff &real, buffInMonster)
		{
			if (real.id == skill.id)
			{//怪物身上已有此buff，无须再次施放技能。
				return false;
			}
		}
	}
	else
	{ 
		foreach(const realBuff &real, buffInRole)
		{
			if (real.id == skill.id)
			{//角色身上已有此buff，无须再次施放技能。
				return false;
			}
		}
	}

	info_buff *buff = nullptr;
	for (quint32 i = 0; i < g_buffList.size(); i++)
	{
		if (g_buffList[i].ID == skill.buff)
		{
			buff = &g_buffList[i];
			break;
		}
	}

	if (buff != nullptr)
	{
		quint32 nA = GetRoleATK(skill.type, bLuck);
		realBuff real;
		real.id = skill.id;
		real.name = skill.name;
		real.icon = skill.icon;
		real.time = nA * buff->time / 100 + 2;
		real.rhp = nA * buff->rhp * skill.level / 100;
		real.ac = nA * buff->ac * skill.level / 100;
		real.mac = nA * buff->mac * skill.level / 100;
		if (skill.buff < 100)
		{//自身增益buff
			nTmp1 = FourCharToInt(myRole->hp_1, myRole->hp_2, myRole->hp_3, myRole->hp_4);
			if (real.rhp > 0 && 0.8 < 1.0 * role_hp_c / nTmp1)
			{
				return false;				//若自身血量大于80%，不使用恢复类buff。
			}
			buffInRole.append(real);
			buffDisp_Role[buffInRole.size() -1 ]->setPixmap(real.icon);
		}
		else
		{//对方减益buff
			buffInMonster.append(real);
			buffDisp_Mon[buffInMonster.size() - 1]->setPixmap(real.icon);
		}
		if (!bCheckConcise)
		{
			QString strTmp = QStringLiteral("<font color=DarkCyan>你</font>使用:<font color=gray>%1</font>").arg(skill.name);
			if (bLuck)
				strTmp += QStringLiteral("获得幸运女神赐福,");

			strTmp += QStringLiteral("  效果持续<font color=magenta>") + QString::number(real.time) + QStringLiteral("</font>回合 ");
#ifdef _DEBUG
			strTmp += QString::number(real.rhp) + " " + QString::number(real.ac) + " " + QString::number(real.mac);
#endif // _DEBUG
		
			ui.edit_display->append(strTmp);
		}
		return true;
	}
	else
	{
		return false;
	}
}

bool fight_fight::MStep_role_Attack(const skill_fight &skill)
{
	qint32 nDamage, nTmp, nTmp1, nTmp2, m_ac,m_mac;
	bool bTmp, bep = false, bLuck = false;
	QList<qint32> ListDamage;

	for (qint32 i = 0; i < skill.times; i++)
	{
		quint32 nA = GetRoleATK(skill.type, bLuck);
		if (skill.type == 2 || skill.type == 3)
		{
			nTmp = nA * skill.damage / 100 + skill.basic;
			m_mac = qMax(0, monster_cur_mac - myRole->equip_secret.macc);
			nDamage = (nTmp - m_mac);
		}
		else
		{
			//不为魔法、道术的一概视为物理攻击。
			nTmp = nA * skill.damage / 100 + skill.basic;
			m_ac = qMax(0, monster_cur_ac - myRole->equip_secret.acc);
			nDamage = (nTmp - m_ac);
		}

		nTmp1 = FourCharToInt(myRole->ep_1, myRole->ep_2, myRole->ep_3, myRole->ep_4);
		nTmp2 = FourCharToInt(myRole->ed_1, myRole->ed_2, myRole->ed_3, myRole->ed_4);
		bTmp = nTmp1 > (qrand() % 10000);
		if (bTmp)
		{	//暴击
			nDamage += nTmp2;
			bep |= bTmp;
		}
		nDamage = (nDamage < 1 ? 1 : nDamage);
		ListDamage.append(nDamage);

		nTmp = monster_cur_hp - nDamage;
		monster_cur_hp = nTmp < 0 ? 0 : nTmp;
		ui.progressBar_monster_hp->setValue(monster_cur_hp);	
	}
	time_remain_monster += skill.stiff;
	if (!bCheckConcise)
	{
		ui.edit_display->append(Generate_Display_LineText(QStringLiteral("你"), skill.name, monster_cur->name, bLuck, bep, ListDamage));
	}
	//更改角色状态
	nTmp = role_hp_c + role_rhp;
	nTmp1 = FourCharToInt(myRole->hp_1, myRole->hp_2, myRole->hp_3, myRole->hp_4);
	role_hp_c = nTmp > nTmp1 ? nTmp1 : nTmp;
	ui.progressBar_role_hp->setValue(role_hp_c);

	nTmp = role_mp_c + role_rmp;
	nTmp1 = FourCharToInt(myRole->mp_1, myRole->mp_2, myRole->mp_3, myRole->mp_4);
	role_mp_c = nTmp > nTmp1 ? nTmp1 : nTmp;
	ui.progressBar_role_mp->setValue(role_mp_c);
	return true;
}
inline void fight_fight::DisplayDropBasic(quint32 nDropExp, quint32 nDropCoin, quint32 nDropRep)
{
	QString strTmp = QStringLiteral("<font color=white>获得\t经验: %1, 金币: %2</font>").arg(nDropExp).arg(nDropCoin);	
	if (bBoss)
	{
		strTmp += QStringLiteral("<font color=white>, 声望: %1 </font>").arg(nDropRep);
	}
	ui.edit_display->append(strTmp);
}

void fight_fight::CreateEquip(itemID id, Info_Equip &DropEquip)
{
	//极品0--8点出现的概率
	double probability[9] = { 0, 0.4096, 0.2048, 0.0512, 0.0128, 0.0032, 0.0016, 0.004, 0.0004 };
	double dTmp = 1.0 * qrand() / RAND_MAX;
	qint32 extraAmount = 0;
	for (int i = 8; i > 0; i--)
	{
		if (dTmp > 1 - probability[i])
		{
			extraAmount = i;
			break;
		}
	}

	EquipExtra extra = { 0 };
	quint32 *p, extraPara = sizeof(EquipExtra) / sizeof(quint32);
	quint32 index, nCount, type;

	//此处强制转换是为了随机化实现极品装备的属性点位置及大小。操作需慎重。
	p = (quint32 *)&extra;

	//分配点数到具体的属性上面。
	while (extraAmount > 0)
	{
		index = qrand() % extraPara;
		nCount = qrand() % g_specialEquip_MaxExtra;

		p[index] = (extraAmount < nCount) ? extraAmount : nCount;
		extraAmount -= p[index];
	}
	//初始化
	DropEquip = { 0 };
	DropEquip.ID = id;
	DropEquip.lvUp = 0;
	DropEquip.extra = extra;

	type = (DropEquip.ID - g_itemID_start_equip) / 1000;
	//所有物品皆不允许有准确加成
	DropEquip.extra.acc = 0;
	//只有项链有幸运加成,并且幸运范围只有0-3。
	if (DropEquip.extra.luck > 0)
	{	//fix 暂时先写死，以后必须在数据库中配置。
		if (type == g_equipType_necklace && (DropEquip.ID == 305006 || DropEquip.ID == 305007 || DropEquip.ID == 305016))
			DropEquip.extra.luck = (DropEquip.extra.luck + 1) / 3;
		else
			DropEquip.extra.luck = 0;
	}
	if (type == g_equipType_weapon || type == g_equipType_necklace || type == g_equipType_ring)
	{
		//武器、项链、戒指不允许有防御、魔御
		DropEquip.extra.ac = 0;
		DropEquip.extra.mac = 0;
	}
	//统计极品点数。
	nCount = DropEquip.extra.luck + DropEquip.extra.ac + DropEquip.extra.mac + DropEquip.extra.dc + DropEquip.extra.mc + DropEquip.extra.sc;
	DropEquip.extraAmount = nCount;
}

void fight_fight::CalcDropItemsAndDisplay(monsterID id)
{
	if (!g_mapDropSet.contains(id))	{
		return;
	}

	Info_Equip DropEquip;
	double dTmp1, dTmp2;
	quint32 nTmp;
	const ListDrop &LD = g_mapDropSet.value(id);
	QStringList List_Pick, List_Drop;
	QString strTmp;
	foreach(const Rational &rRat, LD)
	{
		dTmp1 = 1.0 * qrand() / RAND_MAX;
		dTmp2 = 1.0 * (rRat.den - 1) / rRat.den;
		if (dTmp1 > dTmp2)
		{
			if (rRat.ID > g_itemID_start_equip && rRat.ID <= g_itemID_stop_equip)
			{
				//掉落装备,大于拾取过滤则拾取，否取丢弃。
				CreateEquip(rRat.ID, DropEquip);
				const Info_basic_equip *equip = Item_Base::GetEquipBasicInfo(DropEquip.ID);
				List_Drop.append(equip->name);

				if (m_bag_equip->size() < g_bag_maxSize && (DropEquip.extraAmount >= FilterAdd) && (equip->lv >= FilterLvl))
				{
					List_Pick.append(equip->name);
					m_bag_equip->append(DropEquip);
				}
			}
			else
			{
				//掉落道具
				const Info_Item *item = Item_Base::FindItem_Item(rRat.ID);
				List_Drop.append(item->name);

				List_Pick.append(item->name);
				m_bag_item->insert(rRat.ID, m_bag_item->value(rRat.ID) + 1);
			}
		}
	}

	if (bBoss)
	{
		//boss额外友情赞助一些道具（一瓶大红，一瓶大蓝，1个银元）
		itemID nArr[5] = { 201003, 201013, 203007 };
		for (quint32 i = 0; i < 3; i++)
		{
			const Info_Item *item = Item_Base::FindItem_Item(nArr[i]);
			List_Drop.append(item->name);
			List_Pick.append(item->name);
			m_bag_item->insert(nArr[i], m_bag_item->value(nArr[i]) + 1);
		}
	}

	if (List_Drop.size() > 0)
	{
		strTmp.clear();
		foreach(const QString &s, List_Drop)
		{
			strTmp += s + ", ";
		}
		ui.edit_display->append(QStringLiteral("<font color=white>啊，大量宝物散落上地上，仔细一看，有%1 好多好多啊。</font>").arg(strTmp));
	}

	if (List_Pick.size() > 0)
	{
		strTmp.clear();
		foreach(const QString &s, List_Pick)
		{
			strTmp += s + ", ";
		}
		ui.edit_display->append(QStringLiteral("<font color=white>拾取 %1 %2，你真挑剔。</font>").arg(strTmp).arg(myRole->name));
	}
}

void fight_fight::Action_role(void)
{
	quint32 nTmp1, nTmp_rhp, nTmp_rmp;

	nTmp1 = myRole->intervel_1 << 8 | myRole->intervel_2;
	time_remain_role += nTmp1;	//累加角色活动时间。

	//使用道具的下限
	nTmp1 = FourCharToInt(myRole->hp_1, myRole->hp_2, myRole->hp_3, myRole->hp_4);
	nTmp_rhp = nTmp1 * ui.edit_hp->text().toInt() / 100;

	nTmp1 = FourCharToInt(myRole->mp_1, myRole->mp_2, myRole->mp_3, myRole->mp_4);
	nTmp_rmp = nTmp1 * ui.edit_mp->text().toInt() / 100;

	//如果勾选了自动使用道具
	if (bCheckHp && role_hp_c < nTmp_rhp)
	{
		Step_role_UsingItem_hp();
	}
	if (bCheckMp && role_mp_c < nTmp_rmp)
	{
		Step_role_UsingItem_mp();
	}

	Step_role_Skill();

	double dTmp;
	quint32 nTmp, nDropExp, nDropCoin, nRoleLevel, nDropRep = 0;
	QString strTmp;

	if (monster_cur_hp <= 0)
	{
		bFighting = false;
		buffInMonster.clear();
		for (int  i = 0; i < MaxBuffCount; i++)
		{
			buffDisp_Mon[i]->setPixmap(QPixmap(""));
		}
		
		//怪物死掉，角色增加经验及金币。若是BOSS，再增加声望。
		//必须先乘1.0转化为double，否则等级相减运算将提升到uint层次从而得到一个无穷大。
		dTmp = atan(0.3 * (1.0 * monster_cur->level - Role_Lvl));
		nTmp = monster_cur->exp * ((dTmp + 1.58) / 2);
		
		//等级每逢99时，经验获取只有1。
		if (Role_Lvl > MaxLevel)
		{
			nDropExp = 0;
		} else if (99 == (Role_Lvl % 100))	{
			nDropExp = 1;
		} else {
			nDropExp = nTmp;
		}
		myRole->exp += nDropExp << 1;
		g_falseRole.exp += nDropExp;

		nDropCoin = monster_cur->exp * 0.1;
		myRole->coin += nDropCoin << 1;
		g_falseRole.coin += nDropCoin;

		if (bBoss)
		{
			nDropRep = nTmp * 0.01;
			myRole->reputation += nDropRep << 1;
			g_falseRole.reputation += nDropRep;
		}

		if (bCheckConcise)
			ui.edit_display->setText(QStringLiteral("<font color=white>你击退了 %1 </font>").arg(monster_cur->name));
		else
			ui.edit_display->append(QStringLiteral("<font color=white>你击退了 %1 </font>").arg(monster_cur->name));

		if (bBoss)	{
			++nCount_boss;
		}	else	{
			++nCount_normalMonster;
		}
		
		nCount_exp += nDropExp;
		nCount_coin += nDropCoin;
		nCount_rep += nDropRep;
		
		ui.edit_display->append("");
		DisplayDropBasic(nDropExp, nDropCoin, nDropRep);
		CalcDropItemsAndDisplay(monster_cur->ID);

		quint64 role_exp = (myRole->exp >> 1) - 1;
		if (role_exp > myRole->lvExp)
		{
			levelUp();
			role_exp = 0;
			ui.edit_display->append(QStringLiteral("<font color=white>升级了. </font>"));
		}
		ui.progressBar_role_exp->setValue(role_exp);
	}
}
void fight_fight::Action_monster(void)
{	
	time_remain_monster += monster_cur->interval;	//累加怪物的活动时间。	

	qint32 monster_dc = monster_cur->DC1 + qrand() % (monster_cur->DC2 - monster_cur->DC1 + 1);	
	qint32 role_ac = role_ac1 + qrand() % (role_ac2 - role_ac1 + 1);	
	qint32 nTmp1 = monster_dc - role_ac;
	double dTmp1 = nTmp1 > 0 ? pow(1.0 * nTmp1 / monster_dc + 0.25,4) : 0;	
	qint32 damage_dc = (monster_dc * dTmp1);

	qint32 monster_mc = monster_cur->MC1 + qrand() % (monster_cur->MC2 - monster_cur->MC1 + 1);
	qint32 role_mac = role_mac1 + qrand() % (role_mac2 - role_mac1 + 1);
	nTmp1 = monster_mc - role_mac;
	dTmp1 = nTmp1 > 0 ? pow(1.0 * nTmp1 / monster_mc + 0.25, 4) : 0;
	qint32 damage_mc = (monster_mc * dTmp1);
	
	qint32 nTmp = (damage_dc > 0 ? damage_dc : 1) + (damage_mc > 0 ? damage_mc : 1);
	role_hp_c -= nTmp;
	if (role_hp_c < 0)
	{
		role_hp_c = 0;
	}
	ui.progressBar_role_hp->setValue(role_hp_c);

	//怪物回血
	monster_cur_hp += monster_cur_rhp;
	if (monster_cur_hp > monster_cur->hp)
		monster_cur_hp = monster_cur->hp;
	if (monster_cur_hp < 0)
		monster_cur_hp = 0;
	ui.progressBar_monster_hp->setValue(monster_cur_hp);

	//非“简洁模式”下显示伤害信息。
	if (!bCheckConcise)
	{
		QList<qint32> list;
		list.append(nTmp);
		ui.edit_display->append(Generate_Display_LineText(monster_cur->name, QStringLiteral("普攻"), QStringLiteral("你"), false, false, list));
	}

	if (role_hp_c <= 0)
	{
		//角色死亡
		bFighting = false;
 		killTimer(nFightTimer);
		++nCount_fail;

		//角色死亡，损失经验30%、金币20%
		quint64 role_exp = (myRole->exp >> 1) - 1;
		quint64 role_coin = (myRole->coin >> 1) - 1;
		quint32 nExp = role_exp * 0.3;
		quint32 nCoin = role_coin * 0.2;
		myRole->exp -= nExp << 1;
		myRole->coin -= nCoin << 1;

		g_falseRole.exp -= nExp;
		g_falseRole.coin -= nCoin;

		ui.progressBar_role_exp->setValue((myRole->exp >> 1) - 1);
		ui.edit_display->append(QStringLiteral("<font color=white>战斗失败!</font>"));
		ui.edit_display->append(QStringLiteral("损失经验：") + QString::number(nExp));
		ui.edit_display->append(QStringLiteral("损失金币：") + QString::number(nCoin));
	}
}

void fight_fight::GenerateMonster()
{
	bBoss = false;
	QString strTmp = "";
	if (bCheckFindBoss && monster_boss_count > 0)
	{
		++nElapse_pre_boss;
		if (nElapse_pre_boss > 100) {
			bBoss = true;
		} else 	{
			bBoss = (1.0 * qrand() / RAND_MAX) > g_fight_boss_probability;
		}
	}
	if (bBoss)
	{
		nElapse_pre_boss = 0;
		qint32 n = qrand() % monster_boss_count;
		monster_cur = &g_MonsterBoss_list[monster_boss_assign[n]];

		strTmp = QStringLiteral("强大的<font color=darkRed>") + monster_cur->name
			+ QStringLiteral("</font>来袭,勇敢地<font color = red>战</font>吧！");
	}
	else
	{
		qint32 n = qrand() % monster_normal_count;
		monster_cur = &g_MonsterNormal_List[monster_normal_assign[n]];

		strTmp = QStringLiteral("<font color= white>遭遇 %1</font>").arg(monster_cur->name);
	}
	if (!bCheckConcise)
	{
		ui.edit_display->setText(strTmp);
	}
}

void fight_fight::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == nXSpeedTimer)
	{
		//检测是否加速
		if (xSpeedTime.elapsed() - nXSpeedInvterval < 0)
		{
			++nXSpeedCount;
			if (nXSpeedCount > 20)	{
				LogIns.append(LEVEL_ERROR, __FUNCTION__, mirErr_XSpeed);
				exit(0);
			}
		} else	{
			--nXSpeedCount;
			if (nXSpeedCount < 0)	{
				nXSpeedCount = 0;
			}
		}
		xSpeedTime.restart();
	}
	else if (event->timerId() == nFightTimer)
	{
		//每一次timerEvent为一个回合。 
		//当前未处于战斗状态，故延时显示上一次的战斗信息。延时完后，生成下一个怪物。
		if (!bFighting)
		{
			--nShowStatusRound;
			if (nShowStatusRound >= 0) 	{
				return;
			}
		
			nShowStatusRound = 10;
			//生成一个怪物，并显示怪物信息。
			GenerateMonster();
			Display_CurrentMonsterInfo();
			bFighting = true;
			time_remain = time_remain_role = time_remain_monster = 0;
		}

		//回合时间已用完，判定战斗超时。
		if (time_remain > nTimeOutTime)
		{
			++nCount_timeout;
			ui.edit_display->append(QStringLiteral("<font color=white>战斗超时，重新寻找怪物。</font>"));
			bFighting = false;
			return;
		}
	
		//若回合时间大于角色时间，则角色活动一回合。再判断，若回合时间小于怪物时间，则怪物活动一回合。
		if (time_remain > time_remain_role)
		{
			Action_role();
			updateRoleBuffInfo();
			updateSkillCD();
		}
		else if (time_remain > time_remain_monster)
		{
			Action_monster();
			updateMonsterBuffInfo();
		}
	
		//战斗记时
		time_remain += nFightInterval;
	}
}

void fight_fight::updateRoleBuffInfo(void)
{
	qint32 i;
	role_rhp = myRole->equip_secret.hpr + Role_Lvl * myRole->equip_secret.ghpr / 100;

	role_ac1 = FourCharToInt(myRole->ac1_1, myRole->ac1_2, myRole->ac1_3, myRole->ac1_4);
	role_ac2 = FourCharToInt(myRole->ac2_1, myRole->ac2_2, myRole->ac2_3, myRole->ac2_4);
	role_mac1 = FourCharToInt(myRole->mac1_1, myRole->mac1_2, myRole->mac1_3, myRole->mac1_4);
	role_mac2 = FourCharToInt(myRole->mac2_1, myRole->mac2_2, myRole->mac2_3, myRole->mac2_4);

	for (i = 0; i < MaxBuffCount && i < buffInRole.size(); i++)
	{
		--buffInRole[i].time;
		if (buffInRole[i].time <= 0)
		{
			buffDisp_Role[i]->setPixmap(QPixmap(""));
			buffInRole.remove(i);
		}
		else
		{
			role_rhp += buffInRole[i].rhp;
			role_ac1 += buffInRole[i].ac;
			role_ac2 += buffInRole[i].ac;
			role_mac1 += buffInRole[i].mac;
			role_mac2 += buffInRole[i].mac;
		}
	}

	i = 0;
	for (; i < buffInRole.size(); i++)
	{
		buffDisp_Role[i]->setPixmap(buffInRole[i].icon);
	}
	for (; i < MaxBuffCount; i++)
	{
		buffDisp_Role[i]->setPixmap(QPixmap(""));
	}
	
	ui.edit_role_rhp->setText(QString::number(role_rhp));
	ui.edit_role_ac->setText(QString("%1-%2").arg(role_ac1).arg(role_ac2));
	ui.edit_role_mac->setText(QString("%1-%2").arg(role_mac1).arg(role_mac2));
}
void fight_fight::updateMonsterBuffInfo(void)
{
	qint32 i,nTmp;
	nTmp = 0;
	monster_cur_ac = monster_cur->AC;
	monster_cur_mac = monster_cur->MAC;

	for (i = 0; i < MaxBuffCount && i < buffInMonster.size(); i++)
	{
		--buffInMonster[i].time;
		if (buffInMonster[i].time <= 0)
		{
			buffDisp_Mon[i]->setPixmap(QPixmap(""));
			buffInMonster.remove(i);
		}
		else
		{
			nTmp -= buffInMonster[i].rhp;
			monster_cur_ac -= buffInMonster[i].ac;
			monster_cur_mac -= buffInMonster[i].mac;
		}
	}

	i = 0;
	for (; i < buffInMonster.size(); i++)
	{
		buffDisp_Mon[i]->setPixmap(buffInMonster[i].icon);
	}
	for (; i < MaxBuffCount; i++)
	{
		buffDisp_Mon[i]->setPixmap(QPixmap(""));
	}

	//如果BOSS没有减血buff,则恢复其原来的回血设置。
	if (nTmp >= 0)
	{
		if (m_mapID < 1000)
		{	//只有普通地图的怪有回血功能。
			monster_cur_rhp = monster_cur->hp >> 7;
		}
		else
		{
			monster_cur_rhp = 0;
		}
	}
	else
		monster_cur_rhp = nTmp;

	if (monster_cur_ac < 0)
	{
		monster_cur_ac = 0;
	}
	if (monster_cur_mac < 0)
	{
		monster_cur_mac = 0;
	}
	ui.edit_monster_rhp->setText(QString::number(monster_cur_rhp));
	ui.edit_monster_ac->setText(QString::number(monster_cur_ac));
	ui.edit_monster_mac->setText(QString::number(monster_cur_mac));
}

void fight_fight::updateSkillCD()
{
	for (int i = 0; i < fightingSkill.size(); i++)
	{
		--fightingSkill[i].cd_c;
	}
}

void fight_fight::levelUp()
{
	g_falseRole.exp = 0;
	g_falseRole.level += 1;

	myRole->exp = 2;
	myRole->level += 2;

	CalcRoleInfo();
}

void fight_fight::CalcRoleInfo(void)
{
	QString strTmp;
	quint32 nTmp1, nTmp2;
	quint64 role_exp;

	Role_Lvl = (myRole->level >> 1) - 1;
	ui.edit_role_level->setText(QStringLiteral("Lv:") + QString::number(Role_Lvl));

	role_rhp = myRole->equip_secret.hpr + Role_Lvl * myRole->equip_secret.ghpr / 100;
	ui.edit_role_rhp->setText(QString::number(role_rhp));

	role_rmp = myRole->equip_secret.mpr + Role_Lvl * myRole->equip_secret.gmpr / 100;
	ui.edit_role_rmp->setText(QString::number(role_rmp));

	myRole->lvExp = g_JobAddSet[Role_Lvl].exp;
	role_exp = (myRole->exp >> 1) - 1;
	ui.progressBar_role_exp->setMaximum(myRole->lvExp);
	if (role_exp >= ui.progressBar_role_exp->maximum())
		ui.progressBar_role_exp->setValue(ui.progressBar_role_exp->maximum());
	else
		ui.progressBar_role_exp->setValue(role_exp);

	nTmp1 = qMax(1000, 1500 - myRole->equip_secret.speed);
	myRole->intervel_1 = (nTmp1 >> 8) & 0xff;
	myRole->intervel_2 = nTmp1 & 0xff;
	ui.edit_role_interval->setText(QString::number(nTmp1));

	const Info_jobAdd &jobAdd = g_JobAddSet[Role_Lvl - 1];

	nTmp1 = jobAdd.dc1 + myRole->equip_add.dc1;
	nTmp2 = jobAdd.dc2 + myRole->equip_add.dc2;
	if (nTmp2 < nTmp1)
	{
		nTmp2 = nTmp1;			//确保上限 >= 下限
	}
	IntToFourChar(nTmp1, myRole->dc1_1, myRole->dc1_2, myRole->dc1_3, myRole->dc1_4);
	IntToFourChar(nTmp2, myRole->dc2_1, myRole->dc2_2, myRole->dc2_3, myRole->dc2_4);
	ui.edit_role_dc->setText(QString("%1-%2").arg(nTmp1).arg(nTmp2));

	nTmp1 = jobAdd.mc1 + myRole->equip_add.mc1;
	nTmp2 = jobAdd.mc2 + myRole->equip_add.mc2;
	if (nTmp2 < nTmp1)
	{
		nTmp2 = nTmp1;
	}
	IntToFourChar(nTmp1, myRole->mc1_1, myRole->mc1_2, myRole->mc1_3, myRole->mc1_4);
	IntToFourChar(nTmp2, myRole->mc2_1, myRole->mc2_2, myRole->mc2_3, myRole->mc2_4);
	ui.edit_role_mc->setText(QString("%1-%2").arg(nTmp1).arg(nTmp2));

	nTmp1 = jobAdd.sc1 + myRole->equip_add.sc1;
	nTmp2 = jobAdd.sc2 + myRole->equip_add.sc2;
	if (nTmp2 < nTmp1)
	{
		nTmp2 = nTmp1;
	}
	IntToFourChar(nTmp1, myRole->sc1_1, myRole->sc1_2, myRole->sc1_3, myRole->sc1_4);
	IntToFourChar(nTmp2, myRole->sc2_1, myRole->sc2_2, myRole->sc2_3, myRole->sc2_4);
	ui.edit_role_sc->setText(QString("%1-%2").arg(nTmp1).arg(nTmp2));

	nTmp1 = jobAdd.ac1 + myRole->equip_add.ac1;
	nTmp2 = jobAdd.ac2 + myRole->equip_add.ac2;
	if (nTmp2 < nTmp1)
	{
		nTmp2 = nTmp1;
	}
	IntToFourChar(nTmp1, myRole->ac1_1, myRole->ac1_2, myRole->ac1_3, myRole->ac1_4);
	IntToFourChar(nTmp2, myRole->ac2_1, myRole->ac2_2, myRole->ac2_3, myRole->ac2_4);
	ui.edit_role_ac->setText(QString("%1-%2").arg(nTmp1).arg(nTmp2));

	nTmp1 = jobAdd.mac1 + myRole->equip_add.mac1;
	nTmp2 = jobAdd.mac2 + myRole->equip_add.mac2;
	if (nTmp2 < nTmp1)
	{
		nTmp2 = nTmp1;
	}
	IntToFourChar(nTmp1, myRole->mac1_1, myRole->mac1_2, myRole->mac1_3, myRole->mac1_4);
	IntToFourChar(nTmp2, myRole->mac2_1, myRole->mac2_2, myRole->mac2_3, myRole->mac2_4);
	ui.edit_role_mac->setText(QString("%1-%2").arg(nTmp1).arg(nTmp2));

	nTmp1 = myRole->equip_add.ep;
	IntToFourChar(nTmp1, myRole->ep_1, myRole->ep_2, myRole->ep_3, myRole->ep_4);

	nTmp1 = myRole->equip_add.ed;
	IntToFourChar(nTmp1, myRole->ed_1, myRole->ed_2, myRole->ed_3, myRole->ed_4);

	myRole->luck_1 = ((myRole->equip_add.luck >> 4) >> 8) & 0xFF;
	myRole->luck_2 = ((myRole->equip_add.luck >> 4) & 0xFF) + myRole->equip_secret.luck;
	g_falseRole.luck = ((myRole->equip_add.luck >> 4) & 0xFF) + myRole->equip_secret.luck;

	myRole->acc = myRole->equip_add.acc & 0xFF;
	myRole->sacred = myRole->equip_add.sacred & 0xFF;

	nTmp1 = jobAdd.hp + myRole->equip_secret.hp + Role_Lvl * myRole->equip_secret.ghp / 100;
	IntToFourChar(nTmp1, myRole->hp_1, myRole->hp_2, myRole->hp_3, myRole->hp_4);
	ui.progressBar_role_hp->setStyleSheet("QProgressBar::chunk { background-color: rgb(255, 0, 0) }");
	ui.progressBar_role_hp->setMaximum(nTmp1);
	ui.progressBar_role_hp->setValue(nTmp1);
	role_hp_c = nTmp1;

	nTmp1 = jobAdd.mp + myRole->equip_secret.mp + Role_Lvl * myRole->equip_secret.gmp / 100;
	IntToFourChar(nTmp1, myRole->mp_1, myRole->mp_2, myRole->mp_3, myRole->mp_4);
	ui.progressBar_role_mp->setStyleSheet("QProgressBar::chunk { background-color: rgb(0, 0, 255) }");
	ui.progressBar_role_mp->setMaximum(nTmp1);
	ui.progressBar_role_mp->setValue(nTmp1);
	role_mp_c = nTmp1;
}

