﻿// 老虎特有技能
#include <LCUI_Build.h>
#include LC_LCUI_H
#include LC_WIDGET_H
#include "../game.h"
#include "game_skill.h"

#define ZSPEED_SPIN_DRILL	1400
#define ZACC_SPIN_DRILL		2000

#define ZSPEED_BOUNCE	550
#define ZACC_BOUNCE	1500
#define XSPEED_BOUNCE	150

static int AttackDamage_SpinDrill( GamePlayer *attacker, GamePlayer *victim, int victim_state )
{
	double damage;
	damage = 20 + attacker->property.punch/5.0;
	return (int)1000;
}

static LCUI_BOOL CanUseSpinDrill( GamePlayer *player )
{
	GamePlayer *attacker;
	/* 该技能不能给非拳击家的角色使用 */
	if( player->type != PLAYER_TYPE_TIGER ) {
		return FALSE;
	}
	if( !player->control.a_attack ) {
		return FALSE;
	}
	attacker = GetSpirntAttackerInCatchRange( player );
	if( attacker ) {
		player->other = attacker;
		return TRUE;
	}
	if( player->state != STATE_CATCH || !player->other ) {
		return FALSE;
	}
	/* 对方需要是被擒住状态 */
	if( player->other->state != STATE_BE_CATCH
	 && player->other->state != STATE_BACK_BE_CATCH ) {
		return FALSE;
	}
	return TRUE;
}

static void AtLandingDone( LCUI_Widget *widget )
{
	GamePlayer *player;

	player = GamePlayer_GetPlayerByWidget( widget );
	GameObject_SetXSpeed( widget, 0 );
	GamePlayer_StartStand( player );
}

static void StartBounce( LCUI_Widget *widget )
{
	GamePlayer *player;

	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_FALL );
	GamePlayer_LockAction( player );
	player->other = NULL;
	GameObject_AtLanding(
		player->object, ZSPEED_BOUNCE,
		-ZACC_BOUNCE, AtLandingDone
	);
	if( GamePlayer_IsLeftOriented(player) ) {
		GameObject_SetXSpeed( player->object, XSPEED_BOUNCE );
	} else {
		GameObject_SetXSpeed( player->object, -XSPEED_BOUNCE );
	}
}

static void AtTargetLandingDone( LCUI_Widget *widget )
{
	int ret = 0;
	GamePlayer *player;

	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_UnlockAction( player );
	ret = GamePlayer_SetLying( player );
	GamePlayer_LockAction( player );
	GamePlayer_StopXMotion( player );
	if( ret == 0 ) {
		GamePlayer_SetRestTimeOut( player, BE_THROW_REST_TIMEOUT, GamePlayer_StartStand );
	}
}

static void StartTargetRightBounce( LCUI_Widget *widget )
{
	GamePlayer *player;

	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_LYING_HIT );
	GameObject_AtActionDone( player->object, ACTION_LYING_HIT, NULL );
	GamePlayer_SetLeftOriented( player );
	GamePlayer_LockAction( player );
	player->other = NULL;
	GameObject_AtLanding(
		player->object, ZSPEED_BOUNCE,
		-ZACC_BOUNCE, AtTargetLandingDone
	);
	GameObject_SetXSpeed( player->object, XSPEED_BOUNCE );
}

static void StartTargetLeftBounce( LCUI_Widget *widget )
{
	GamePlayer *player;

	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_LYING_HIT );
	GameObject_AtActionDone( player->object, ACTION_LYING_HIT, NULL );
	GamePlayer_SetRightOriented( player );
	GamePlayer_LockAction( player );
	player->other = NULL;
	GameObject_AtLanding(
		player->object, ZSPEED_BOUNCE,
		-ZACC_BOUNCE, AtTargetLandingDone
	);
	GameObject_SetXSpeed( player->object, -XSPEED_BOUNCE );
}

static void StartSpinDrill( GamePlayer *player )
{
	int z_index;
	if( !player->other ) {
		return;
	}
	GamePlayer_UnlockAction( player->other );
	GamePlayer_ChangeState( player->other, STATE_HALF_LYING );
	GameObject_SetX( player->other->object, GameObject_GetX(player->object) );
	GameObject_SetY( player->other->object, GameObject_GetY(player->object) );
	GamePlayer_LockAction( player->other );
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_CATCH_SKILL_FA );
	GamePlayer_LockAction( player );
	z_index = Widget_GetZIndex( player->other->object );
	Widget_SetZIndex( player->object, z_index+1 );
	GameObject_AtLanding( player->object, ZSPEED_SPIN_DRILL, -ZACC_SPIN_DRILL, StartBounce );
	if( GamePlayer_IsLeftOriented(player) ) {
		GameObject_AtLanding( player->other->object, ZSPEED_SPIN_DRILL, -ZACC_SPIN_DRILL, StartTargetLeftBounce );
	} else {
		GameObject_AtLanding( player->other->object, ZSPEED_SPIN_DRILL, -ZACC_SPIN_DRILL, StartTargetRightBounce );
	}
}

/** 注册老虎特有的技能 */
void TigerSkill_Register(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_SPIN_DRILL,
				SKILLPRIORITY_SPIN_DRILL,
				CanUseSpinDrill,
				StartSpinDrill
	);
	AttackLibrary_AddAttack( ATK_SPIN_DRILL, AttackDamage_SpinDrill, NULL );
}