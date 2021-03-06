// built-in include guard removal
// just in case the user has a local dependency with the same file name
#if defined _inc_bag
	#undef _inc_bag
#endif
// custom include-guard to ensure we don't duplicate
#if defined _bag_included
	#endinput
#endif
#define _bag_included


#include <a_samp>
#include <logger>
#include <YSI\y_timers>
#include <mathutil>
#include <personal-space>
#include <language>
#include <item>
#include <item-array-data>
#include <container>
#include <inventory>
#include <inventory-dialog>
#include <container-dialog>

#include <YSI\y_hooks>


#if !defined MAX_BAG_TYPE
	#define MAX_BAG_TYPE (BagType:10)
#endif

#if !defined BAG_ATTACH_INDEX
	#define BAG_ATTACH_INDEX (9)
#endif

#define INVALID_BAG_TYPE (BagType:-1)

enum E_BAG_TYPE_DATA {
	ItemType:bag_itemtype,
	bag_size,
	Float:bag_attachOffsetX,
	Float:bag_attachOffsetY,
	Float:bag_attachOffsetZ,
	Float:bag_attachRotX,
	Float:bag_attachRotY,
	Float:bag_attachRotZ,
	Float:bag_attachScaleX,
	Float:bag_attachScaleY,
	Float:bag_attachScaleZ
}

static
	bag_TypeData[MAX_BAG_TYPE][E_BAG_TYPE_DATA],
	BagType:bag_TypeTotal,
	BagType:bag_ItemTypeBagType[MAX_ITEM_TYPE] = {INVALID_BAG_TYPE, ...},

	Item:bag_ContainerItem[MAX_CONTAINER],
	bag_ContainerPlayer[MAX_CONTAINER],

	Item:bag_PlayerBagItem[MAX_PLAYERS],
	bag_InventoryOptionID[MAX_PLAYERS],
	bool:bag_PuttingInBag[MAX_PLAYERS],
	bool:bag_TakingOffBag[MAX_PLAYERS],
	Item:bag_CurrentBag[MAX_PLAYERS],
	Timer:bag_BagRummageEnter[MAX_PLAYERS],
	Timer:bag_BagRummageTimer[MAX_PLAYERS],
	bag_LookingInBag[MAX_PLAYERS];


forward OnPlayerWearBag(playerid, itemid);
forward OnPlayerRemoveBag(playerid, itemid);


hook OnScriptInit() {
	new languageid = InitLanguage("English");
	AddLanguageEntry(languageid, "CNTEXTRASLO", "Extra %d slots required");
	AddLanguageEntry(languageid, "BAGEXTRASLO", "Extra %d bag slots required");
	AddLanguageEntry(languageid, "BAGITMADDED", "Item added to bag");

	for(new Container:i; i < MAX_CONTAINER; i++) {
		bag_ContainerPlayer[i] = INVALID_PLAYER_ID;
		bag_ContainerItem[i] = INVALID_ITEM_ID;
	}
}

hook OnPlayerConnect(playerid) {
	bag_PlayerBagItem[playerid] = INVALID_ITEM_ID;
	bag_PuttingInBag[playerid] = false;
	bag_TakingOffBag[playerid] = false;
	bag_CurrentBag[playerid] = INVALID_ITEM_ID;
	bag_LookingInBag[playerid] = INVALID_PLAYER_ID;
}


/*==============================================================================

	Core

==============================================================================*/


stock BagType:DefineBagType(
	ItemType:itemtype,
	size,
	Float:attachOffsetX,
	Float:attachOffsetY,
	Float:attachOffsetZ,
	Float:attachRotX,
	Float:attachRotY,
	Float:attachRotZ,
	Float:attachScaleX,
	Float:attachScaleY,
	Float:attachScaleZ
) {
	if(bag_TypeTotal == MAX_BAG_TYPE - BagType:1) {
		fatal("maximum amount of bags defined");
		return INVALID_BAG_TYPE;
	}

	SetItemTypeMaxArrayData(itemtype, 2);

	bag_TypeData[bag_TypeTotal][bag_itemtype]		= itemtype;
	bag_TypeData[bag_TypeTotal][bag_size]			= size;
	bag_TypeData[bag_TypeTotal][bag_attachOffsetX]	= attachOffsetX;
	bag_TypeData[bag_TypeTotal][bag_attachOffsetY]	= attachOffsetY;
	bag_TypeData[bag_TypeTotal][bag_attachOffsetZ]	= attachOffsetZ;
	bag_TypeData[bag_TypeTotal][bag_attachRotX]		= attachRotX;
	bag_TypeData[bag_TypeTotal][bag_attachRotY]		= attachRotY;
	bag_TypeData[bag_TypeTotal][bag_attachRotZ]		= attachRotZ;
	bag_TypeData[bag_TypeTotal][bag_attachScaleX]	= attachScaleX;
	bag_TypeData[bag_TypeTotal][bag_attachScaleY]	= attachScaleY;
	bag_TypeData[bag_TypeTotal][bag_attachScaleZ]	= attachScaleZ;

	bag_ItemTypeBagType[itemtype] = bag_TypeTotal;

	dbg("bag", "defined bag type", _i("itemtype", _:itemtype));

	return bag_TypeTotal += BagType:1;
}

stock GivePlayerBag(playerid, Item:itemid) {
	if(!IsValidItem(itemid)) {
		return 1;
	}

	new BagType:bagtype = bag_ItemTypeBagType[GetItemType(itemid)];

	if(bagtype == INVALID_BAG_TYPE) {
		return 2;
	}

	new
		Container:containerid,
		Error:e;

	e = GetItemArrayDataAtCell(itemid, _:containerid, 1);
	if(IsError(e)) {
		err("failed to get array data at cell for bag",
			_i("itemid", _:itemid),
			_i("containerid", _:containerid));
		Handled();
		return 3;
	}

	if(!IsValidContainer(containerid)) {
		err("bag containerid was invalid and will be recreated",
			_i("itemid", _:itemid),
			_i("containerid", _:containerid));

		_bag_createContainerForItem(itemid);
	}

	new ItemType:itemtype = bag_TypeData[bagtype][bag_itemtype];

	new colour;
	GetItemTypeColour(itemtype, colour);

	bag_PlayerBagItem[playerid] = itemid;

	new modelid;
	GetItemTypeModel(itemtype, modelid);

	new ret = SetPlayerAttachedObject(playerid, BAG_ATTACH_INDEX, modelid, 1,
		bag_TypeData[bagtype][bag_attachOffsetX],
		bag_TypeData[bagtype][bag_attachOffsetY],
		bag_TypeData[bagtype][bag_attachOffsetZ],
		bag_TypeData[bagtype][bag_attachRotX],
		bag_TypeData[bagtype][bag_attachRotY],
		bag_TypeData[bagtype][bag_attachRotZ],
		bag_TypeData[bagtype][bag_attachScaleX],
		bag_TypeData[bagtype][bag_attachScaleY],
		bag_TypeData[bagtype][bag_attachScaleZ], colour, colour);

	bag_ContainerItem[containerid] = itemid;
	bag_ContainerPlayer[containerid] = playerid;
	RemoveItemFromWorld(itemid);
	RemoveCurrentItem(GetItemHolder(itemid));

	dbg("bag", "given bag to player",
		_i("ret", ret),
		_i("playerid", playerid),
		_i("itemid", _:itemid),
		_i("itemtype", _:itemtype),
		_i("bagtype", _:bagtype),
		_i("modelid", modelid)
	);

	return 0;
}

stock RemovePlayerBag(playerid) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	if(!IsValidItem(bag_PlayerBagItem[playerid])) {
		return 2;
	}

	new
		Container:containerid,
		Error:e;

	e = GetItemArrayDataAtCell(bag_PlayerBagItem[playerid], _:containerid, 1);
	if(e) {
		err("failed to get array data at cell for bag",
			_i("itemid", _:bag_PlayerBagItem[playerid]),
			_i("containerid", _:containerid));
		Handled();

		return 3;
	}

	if(!IsValidContainer(containerid)) {
		err("bag containerid was invalid and will be recreated",
			_i("itemid", _:bag_PlayerBagItem[playerid]),
			_i("containerid", _:containerid));

		_bag_createContainerForItem(bag_PlayerBagItem[playerid]);
	}

	RemovePlayerAttachedObject(playerid, BAG_ATTACH_INDEX);
	CreateItemInWorld(bag_PlayerBagItem[playerid], 0.0, 0.0, 0.0, .world = GetPlayerVirtualWorld(playerid), .interior = GetPlayerInterior(playerid));

	dbg("bag", "removed player bag",
		_i("playerid", playerid),
		_i("itemid", _:bag_PlayerBagItem[playerid])
	);

	bag_ContainerPlayer[containerid] = INVALID_PLAYER_ID;
	bag_PlayerBagItem[playerid] = INVALID_ITEM_ID;

	return 0;
}

stock DestroyPlayerBag(playerid) {
	if(!(0 <= playerid < MAX_PLAYERS)) {
		return 1;
	}

	if(!IsValidItem(bag_PlayerBagItem[playerid])) {
		return 2;
	}

	new
		Container:containerid,
		Error:e;

	e = GetItemArrayDataAtCell(bag_PlayerBagItem[playerid], _:containerid, 1);
	if(IsError(e)) {
		err("failed to get array data at cell for bag",
			_i("itemid", _:bag_PlayerBagItem[playerid]),
			_i("containerid", _:containerid));
		Handled();
		return 3;
	}

	if(IsValidContainer(containerid)) {
		bag_ContainerPlayer[containerid] = INVALID_PLAYER_ID;
		DestroyContainer(containerid);
	}

	RemovePlayerAttachedObject(playerid, BAG_ATTACH_INDEX);
	DestroyItem(bag_PlayerBagItem[playerid]);

	dbg("bag", "destroyed player bag",
		_i("playerid", playerid),
		_i("itemid", _:bag_PlayerBagItem[playerid])
	);

	bag_PlayerBagItem[playerid] = INVALID_ITEM_ID;

	return 0;
}

stock ShowPlayerBag(playerid) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	new Item:itemid = bag_PlayerBagItem[playerid];

	if(!IsValidItem(itemid)) {
		return 2;
	}

	return _bag_displayUI(playerid, itemid, false);
}

// Automatically determines whether to add to the player's inventory or bag.
stock AddItemToPlayer(playerid, Item:itemid, useinventory = false, playeraction = true) {
	new ItemType:itemtype = GetItemType(itemid);

	if(IsItemTypeCarry(itemtype)) {
		return -1;
	}

	new required;

	if(useinventory) {
		required = AddItemToInventory(playerid, itemid);
		if(required == 0) {
			return 0;
		}
	}

	if(!IsValidItem(bag_PlayerBagItem[playerid])) {
		if(required > 0) {
			new message[128];
			format(message, sizeof(message), @L(playerid, "CNTEXTRASLO", true), required);
			ShowActionText(playerid, message, 3000, 150);
		}

		return -3;
	}

	new
		Container:containerid,
		Error:e;
	e = GetItemArrayDataAtCell(bag_PlayerBagItem[playerid], _:containerid, 1);
	if(IsError(e)) {
		Handled();
		return -4;
	}

	if(!IsValidContainer(containerid)) {
		return -5;
	}

	new itemsize;
	if(GetItemTypeSize(GetItemType(itemid), itemsize) != 0) {
		return -6;
	}

	new freeslots;
	if(GetContainerFreeSlots(containerid, freeslots) != 0) {
		return -7;
	}

	if(itemsize > freeslots) {
		new message[128];
		format(message, sizeof message, @L(playerid, "BAGEXTRASLO", true), itemsize - freeslots);
		ShowActionText(playerid, message, 3000, 150);
		return -8;
	}

	if(playeraction) {
		ShowActionText(playerid, @L(playerid, "BAGITMADDED", true), 3000, 150);
		ApplyAnimation(playerid, "PED", "PHONE_IN", 4.0, 1, 0, 0, 0, 300);
		bag_PuttingInBag[playerid] = true;
		defer bag_PutItemIn(playerid, _:itemid, _:containerid);
	} else {
		return AddItemToContainer(containerid, itemid, playerid);
	}

	return 0;
}

stock bool:IsItemTypeBag(ItemType:itemtype) {
	if(!IsValidItemType(itemtype)) {
		return false;
	}

	return bag_ItemTypeBagType[itemtype] != INVALID_BAG_TYPE;
}

stock BagType:GetItemBagType(ItemType:itemtype) {
	if(!IsValidItemType(itemtype)) {
		return INVALID_BAG_TYPE;
	}

	return bag_ItemTypeBagType[itemtype];
}

stock GetPlayerBagItem(playerid) {
	if(!(0 <= playerid < MAX_PLAYERS)) {
		return INVALID_ITEM_ID;
	}

	return bag_PlayerBagItem[playerid];
}

stock GetContainerPlayerBag(Container:containerid) {
	if(!IsValidContainer(containerid)) {
		return INVALID_PLAYER_ID;
	}

	return bag_ContainerPlayer[containerid];
}

stock Item:GetContainerBagItem(Container:containerid) {
	if(!IsValidContainer(containerid)) {
		return INVALID_ITEM_ID;
	}

	return bag_ContainerItem[containerid];
}

stock GetBagItemContainerID(itemid, &Container:containerid) {
	if(!IsItemTypeBag(GetItemType(itemid))) {
		return INVALID_CONTAINER_ID;
	}

	return GetItemArrayDataAtCell(itemid, 1, containerid);
}


/*==============================================================================

	Internal Functions and Hooks

==============================================================================*/


_bag_createContainerForItem(Item:itemid) {
	new
		ItemType:itemType = GetItemType(itemid),
		BagType:bagtype,
		Container:containerid;

	bagtype = bag_ItemTypeBagType[itemType];

	if(bagtype == INVALID_BAG_TYPE) {
		err("attempt to create bag container for item that is not a bag");
		return 1;
	}

	new name[MAX_ITEM_NAME];
	GetItemTypeName(itemType, name);
	containerid = CreateContainer(name, bag_TypeData[bagtype][bag_size]);

	bag_ContainerItem[containerid] = itemid;
	bag_ContainerPlayer[containerid] = INVALID_PLAYER_ID;

	SetItemArrayDataSize(itemid, 2);
	SetItemArrayDataAtCell(itemid, _:containerid, 1);

	dbg("bag", "created container for bag",
		_i("itemid", _:itemid),
		_i("containerid", _:containerid)
	);

	return 0;
}

hook OnItemCreate(Item:itemid) {
	new BagType:bagtype = bag_ItemTypeBagType[GetItemType(itemid)];

	if(bagtype != INVALID_BAG_TYPE) {
		_bag_createContainerForItem(itemid);
	}
}

hook OnItemCreateInWorld(Item:itemid) {
	if(IsItemTypeBag(GetItemType(itemid))) {
		new Button:buttonid;
		GetItemButtonID(itemid, buttonid);
		SetButtonText(buttonid, "Hold "ITEM_GAMETEXT_INTERACT" to pick up~n~Press "ITEM_GAMETEXT_INTERACT" to open");
	}
}

hook OnItemDestroy(Item:itemid) {
	if(IsItemTypeBag(GetItemType(itemid))) {
		new
			Container:containerid,
			Error:e;
		e = GetItemArrayDataAtCell(itemid, _:containerid, 1);
		if(IsError(e)) {
			err("failed data access");
			Handled();
			return Y_HOOKS_CONTINUE_RETURN_1;
		}

		if(IsValidContainer(containerid)) {
			bag_ContainerPlayer[containerid] = INVALID_PLAYER_ID;
			bag_ContainerItem[containerid] = INVALID_ITEM_ID;
			DestroyContainer(containerid);
		}
	}

	return Y_HOOKS_CONTINUE_RETURN_1;
}

hook OnPlayerUseItem(playerid, Item:itemid) {
	if(bag_ItemTypeBagType[GetItemType(itemid)] != INVALID_BAG_TYPE) {
		// is the player inside a container view? is the item usage, out of world?
		// if(IsValidContainer(GetPlayerCurrentContainer(playerid)))
		//	return Y_HOOKS_CONTINUE_RETURN_0;

		if(IsItemInWorld(itemid)) {
			_bag_displayUI(playerid, itemid, true);
		} else {
			_bag_displayUI(playerid, itemid, false);
		}

		return Y_HOOKS_BREAK_RETURN_1;
	}

	return Y_HOOKS_CONTINUE_RETURN_0;
}

hook OnPlayerUseItemWithItem(playerid, Item:itemid, Item:withitemid) {
	if(bag_ItemTypeBagType[GetItemType(withitemid)] != INVALID_BAG_TYPE) {
		_bag_displayUI(playerid, withitemid, true);
		return Y_HOOKS_BREAK_RETURN_1;
	}

	return Y_HOOKS_CONTINUE_RETURN_0;
}

hook OnPlayerKeyStateChange(playerid, newkeys, oldkeys) {
	if(GetPlayerSpecialAction(playerid) == SPECIAL_ACTION_CUFFED || GetPlayerAnimationIndex(playerid) == 1381) {
		return Y_HOOKS_CONTINUE_RETURN_1;
	}

	if(IsPlayerInAnyVehicle(playerid)) {
		return Y_HOOKS_CONTINUE_RETURN_1;
	}

	if(newkeys & KEY_YES) {
		_bag_equip(playerid);
	} else if(newkeys & KEY_NO) {
		_bag_drop(playerid);
	} else if(newkeys & 16) {
		_bag_rummage(playerid);
	}

	return Y_HOOKS_CONTINUE_RETURN_1;
}

_bag_equip(playerid) {
	new Item:itemid = GetPlayerItem(playerid);

	if(!IsValidItem(itemid)) {
		return 1;
	}

	if(bag_PuttingInBag[playerid]) {
		return 2;
	}

	new ItemType:itemtype = GetItemType(itemid);

	if(IsItemTypeBag(itemtype)) {
		if(IsValidItem(bag_PlayerBagItem[playerid])) {
			new Item:currentbagitem = bag_PlayerBagItem[playerid];

			RemovePlayerBag(playerid);
			GivePlayerBag(playerid, itemid);
			GiveWorldItemToPlayer(playerid, currentbagitem, 1);
		} else {
			if(CallLocalFunction("OnPlayerWearBag", "dd", playerid, _:itemid)) {
				return 3;
			}

			GivePlayerBag(playerid, itemid);
		}
	} else {
		new ret = AddItemToPlayer(playerid, itemid);
		dbg("bag", "AddItemToPlayer", _i("ret", ret));
	}

	return 0;
}

_bag_drop(playerid) {
	if(!IsValidItem(bag_PlayerBagItem[playerid])) {
		return 1;
	}

	if(IsValidItem(GetPlayerItem(playerid))) {
		return 2;
	}

	if(IsValidItem(GetPlayerInteractingItem(playerid))) {
		return 3;
	}

	if(CallLocalFunction("OnPlayerRemoveBag", "dd", playerid, _:bag_PlayerBagItem[playerid])) {
		return 4;
	}

	new
		Container:containerid,
		Error:e;
	e = GetItemArrayDataAtCell(bag_PlayerBagItem[playerid], _:containerid, 1);
	if(IsError(e)) {
		Handled();
		return 5;
	}

	RemovePlayerAttachedObject(playerid, BAG_ATTACH_INDEX);
	CreateItemInWorld(bag_PlayerBagItem[playerid], 0.0, 0.0, 0.0, .world = GetPlayerVirtualWorld(playerid), .interior = GetPlayerInterior(playerid));
	GiveWorldItemToPlayer(playerid, bag_PlayerBagItem[playerid], 1);
	bag_ContainerPlayer[containerid] = INVALID_PLAYER_ID;
	bag_PlayerBagItem[playerid] = INVALID_ITEM_ID;
	bag_TakingOffBag[playerid] = true;

	return 0;
}

_bag_rummage(playerid) {
	new
		targetid = INVALID_PLAYER_ID,
		players[MAX_PLAYERS],
		totalPlayers;

	totalPlayers = GetPlayersNextToPlayer(playerid, players);

	if(totalPlayers == 0) {
		return 1;
	}

	for(new i; i < totalPlayers; ++i) {
		if(IsPlayerInAnyVehicle(i)) {
			continue;
		}

		targetid = i;
		break;
	}

	new
		Float:px,
		Float:py,
		Float:pz,
		Float:tx,
		Float:ty,
		Float:tz,
		Float:tr,
		Float:angle;

	GetPlayerPos(playerid, px, py, pz);
	GetPlayerPos(targetid, tx, ty, tz);
	GetPlayerFacingAngle(targetid, tr);

	angle = GetAbsoluteAngle(tr - GetAngleToPoint(tx, ty, px, py));

	if(155.0 < angle < 205.0) {
		CancelPlayerMovement(playerid);
		bag_BagRummageEnter[playerid] = defer _bag_rummageDefer(playerid, targetid);
	}

	return 0;
}

timer bag_PutItemIn[300](playerid, itemid, containerid) {
	AddItemToContainer(Container:containerid, Item:itemid, playerid);
	bag_PuttingInBag[playerid] = false;
}

timer _bag_rummageDefer[250](playerid, targetid) {
	_bag_displayUI(playerid, bag_PlayerBagItem[targetid], false);
	bag_LookingInBag[playerid] = targetid;
}

hook OnPlayerLeavePlayer(playerid, targetid) {
	if(bag_LookingInBag[playerid] == targetid || bag_LookingInBag[targetid] == playerid) {
		ClosePlayerContainer(playerid);
		CancelSelectTextDraw(playerid);
		bag_LookingInBag[playerid] = -1;
	}
}

_bag_displayUI(playerid, Item:itemid, bool:animation) {
	new Container:containerid;
	GetItemArrayDataAtCell(itemid, _:containerid, 1);
	DisplayContainerInventory(playerid, containerid);
	bag_CurrentBag[playerid] = itemid;

	if(animation) {
		ApplyAnimation(playerid, "BOMBER", "BOM_PLANT_IN", 4.0, 0, 0, 0, 1, 0);
	} else {
		CancelPlayerMovement(playerid);
	}

	return 0;
}

hook OnItemAddToInventory(playerid, Item:itemid, slot) {
	new ItemType:itemtype = GetItemType(itemid);

	if(IsItemTypeBag(itemtype)) {
		return Y_HOOKS_BREAK_RETURN_1;
	}

	if(IsItemTypeCarry(itemtype)) {
		return Y_HOOKS_BREAK_RETURN_1;
	}

	return Y_HOOKS_CONTINUE_RETURN_0;
}

hook OnPlayerAddToInventory(playerid, Item:itemid, success) {
	if(success) {
		new ItemType:itemtype = GetItemType(itemid);

		if(IsItemTypeBag(itemtype)) {
			return Y_HOOKS_BREAK_RETURN_1;
		}

		if(IsItemTypeCarry(itemtype)) {
			return Y_HOOKS_BREAK_RETURN_1;
		}
	} else {
		new ItemType:itemtype = GetItemType(itemid);

		if(IsItemTypeBag(itemtype)) {
			return Y_HOOKS_BREAK_RETURN_1;
		}

		if(IsItemTypeCarry(itemtype)) {
			return Y_HOOKS_BREAK_RETURN_1;
		}

		new
			message[128],
			itemsize,
			freeslots;

		GetItemTypeSize(GetItemType(itemid), itemsize);
		GetInventoryFreeSlots(playerid, freeslots);

		format(message, sizeof message, @L(playerid, "CNTEXTRASLO", true), itemsize - freeslots);
		ShowActionText(playerid, message, 3000, 150);
	}

	return Y_HOOKS_CONTINUE_RETURN_0;
}

hook OnPlayerCloseContainer(playerid, containerid) {
	if(IsValidItem(bag_CurrentBag[playerid])) {
		ClearAnimations(playerid);
		bag_CurrentBag[playerid] = INVALID_ITEM_ID;
		bag_LookingInBag[playerid] = -1;
	}

	stop bag_BagRummageTimer[playerid];
}

hook OnPlayerDropItem(playerid, Item:itemid) {
	if(IsItemTypeBag(GetItemType(itemid))) {
		if(bag_TakingOffBag[playerid]) {
			bag_TakingOffBag[playerid] = false;
			return Y_HOOKS_BREAK_RETURN_1;
		}
	}

	return Y_HOOKS_CONTINUE_RETURN_0;
}

hook OnPlayerGiveItem(playerid, targetid, Item:itemid) {
	if(IsItemTypeBag(GetItemType(itemid))) {
		if(bag_TakingOffBag[playerid]) {
			bag_TakingOffBag[playerid] = false;
			return Y_HOOKS_BREAK_RETURN_1;
		}
	}

	return Y_HOOKS_CONTINUE_RETURN_0;
}

hook OnPlayerViewInvOpt(playerid) {
	new Container:containerid;
	GetPlayerCurrentContainer(playerid, containerid);
	if(IsValidItem(bag_PlayerBagItem[playerid]) && !IsValidContainer(containerid)) {
		bag_InventoryOptionID[playerid] = AddInventoryOption(playerid, "Move to bag");
	}
}

hook OnPlayerSelectInvOpt(playerid, option) {
	new Container:currContainerid;
	GetPlayerCurrentContainer(playerid, currContainerid);
	if(IsValidItem(bag_PlayerBagItem[playerid]) && !IsValidContainer(currContainerid)) {
		if(option == bag_InventoryOptionID[playerid]) {
			new
				Container:containerid,
				slot,
				Item:itemid,
				Error:e;

			slot = GetPlayerSelectedInventorySlot(playerid);
			e = GetItemArrayDataAtCell(bag_PlayerBagItem[playerid], _:containerid, 1);
			if(IsError(e)) {
				err("failed to get required data for inventory transfer",
					_i("playerid", playerid));
				Handled();
				return Y_HOOKS_CONTINUE_RETURN_0;
			}

			new ret = GetInventorySlotItem(playerid, slot, itemid);
			if(ret) {
				err("failed to get inventory slot item for transfer",
					_i("playerid", playerid));
				return Y_HOOKS_CONTINUE_RETURN_0;
			}

			if(!IsValidItem(itemid)) {
				DisplayPlayerInventory(playerid);
				return Y_HOOKS_CONTINUE_RETURN_0;
			}

			new required = AddItemToContainer(containerid, itemid, playerid);

			if(required > 0) {
				new message[128];
				format(message, sizeof message, @L(playerid, "BAGEXTRASLO", true), required);
				ShowActionText(playerid, message, 3000, 150);
			}

			DisplayPlayerInventory(playerid);
		}
	}

	return Y_HOOKS_CONTINUE_RETURN_0;
}

hook OnPlayerViewCntOpt(playerid, Container:containerid) {
	new
		Container:bagContainer,
		Error:e;
	e = GetItemArrayDataAtCell(bag_PlayerBagItem[playerid], _:bagContainer, 1);
	if(IsError(e)) {
		err("failed to get item array data at cell",
			_i("itemid", _:bag_PlayerBagItem[playerid]));
		Handled();
		return Y_HOOKS_CONTINUE_RETURN_1;
	}

	if(IsValidItem(bag_PlayerBagItem[playerid]) && containerid != bagContainer) {
		bag_InventoryOptionID[playerid] = AddContainerOption(playerid, "Move to bag");
	}

	return Y_HOOKS_CONTINUE_RETURN_1;
}

hook OnPlayerSelectCntOpt(playerid, Container:containerid, option) {
	new
		Container:bagContainer,
		Error:e;

	e = GetItemArrayDataAtCell(bag_PlayerBagItem[playerid], _:bagContainer, 1);
	if(IsError(e)) {
		Handled();
		return Y_HOOKS_CONTINUE_RETURN_1;
	}

	if(!IsValidItem(bag_PlayerBagItem[playerid])) {
		return Y_HOOKS_CONTINUE_RETURN_1;
	}

	if(containerid != bagContainer) {
		if(option == bag_InventoryOptionID[playerid]) {
			new
				Container:bagcontainerid,
				slot,
				Item:itemid;

			GetItemArrayDataAtCell(bag_PlayerBagItem[playerid], _:bagcontainerid, 1);
			GetPlayerContainerSlot(playerid, slot);
			GetContainerSlotItem(containerid, slot, itemid);

			if(!IsValidItem(itemid)) {
				DisplayContainerInventory(playerid, containerid);
				return Y_HOOKS_CONTINUE_RETURN_0;
			}

			new required = AddItemToContainer(bagcontainerid, itemid, playerid);

			if(required > 0) {
				new message[128];
				format(message, sizeof message, @L(playerid, "BAGEXTRASLO", true), required);
				ShowActionText(playerid, message, 3000, 150);
			}

			DisplayContainerInventory(playerid, containerid);
		}
	}

	return Y_HOOKS_CONTINUE_RETURN_0;
}

hook OnItemAddToContainer(Container:containerid, Item:itemid, playerid) {
	if(GetContainerBagItem(containerid) != INVALID_ITEM_ID) {
		if(IsItemTypeCarry(GetItemType(itemid))) {
			return Y_HOOKS_BREAK_RETURN_1;
		}
	}

	return Y_HOOKS_CONTINUE_RETURN_0;
}

static stock CancelPlayerMovement(playerid) {
	new
		Float:x,
		Float:y,
		Float:z;

	GetPlayerPos(playerid, x, y, z);
	SetPlayerPos(playerid, x, y, z);
	ClearAnimations(playerid);
	TogglePlayerControllable(playerid, true);
}
