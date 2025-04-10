#include "UI_Manager.h"
#include "Custom_behaviours.h"


UI_Manager::UI_Manager()
{
    ui_cam = {0};
    ui_cam.target  = {0,0};
    ui_cam.offset = {0,0};
    ui_cam.zoom = 2.0f;// global zoom for pixel perfect

#pragma region main_Inv_ui
    auto inv_ui = std::make_unique<Entity>("ui_parent");
    inv_ui->addComponent<TransformComp>(0,0);

    auto inventory = std::make_unique<Entity>("Inv_slots");
    inventory->addComponent<TransformComp>(0,0);

    auto bag = std::make_unique<Entity>("bag");
    

    bag->addComponent<TransformComp>(TransformComp(5,227));
    bag->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {0,32,42,29}));
    bag->addComponent<ColliderComp>(ColliderComp(0,0,0,0));
    auto on_inv = bag->addComponent<OnInvButton>();
    on_inv->inventory = inventory.get();
    on_inv->Start();
    inv_man = inventory->addComponent<InventoryManager>();

    inv_man->slots.clear(); // clear slots

    for(int i = 0; i < 10; i++){
        auto inv = std::make_unique<Entity>("Inv");
        inv->addComponent<TransformComp>(TransformComp(62 + i* 32,227));
        inv->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {182,0,30,30}));
        inv->addComponent<ColliderComp>(0,0,0,0);
        auto slot = inv->addComponent<Slot_script>();
        inv_man->slots.push_back(slot);



        slot->idx = i;
        

        auto slot_icon = std::make_unique<Entity>("slot_icon" + i);
        auto slot_icon_t = slot_icon->addComponent<TransformComp>();
        slot_icon->addComponent<Sprite>(Sprite(ResourcesLoader::inv_items, {0,0,24,24}));
        slot_icon_t->position = {3,3};
        slot_icon->setActive(false);


        inv->addChild(std::move(slot_icon));
        inventory->addChild(std::move(inv));
    }
    
    //inv_man->ChildInitialized(); // call a function
    inventory->getComponent<TransformComp>()->position.y = 30;

    auto craft_btn = std::make_unique<Entity>("Craft_btn");
    craft_btn->addComponent<TransformComp>(TransformComp(396,224));
    craft_btn->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {126,0,56,32}));
    craft_btn->addComponent<ColliderComp>(ColliderComp(0,0,0,0));
    craft_btn->addComponent<Craft_btn_script>();

    auto craft_icon = std::make_unique<Entity>("craft_icon");
    craft_icon->addComponent<TransformComp>(TransformComp(15,2));
    auto spc = Sprite(ResourcesLoader::ui_page, {242,0,28,27});
    craft_icon->addComponent<Sprite>(spc);
    craft_btn->addChild(std::move(craft_icon));

    auto lens_btn = std::make_unique<Entity>("Lens_btn");
    lens_btn->addComponent<TransformComp>(TransformComp(455,224));
    lens_btn->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {126,0,56,32}));
    lens_btn->addComponent<ColliderComp>(ColliderComp(0,0,0,0));
    lens_btn->addComponent<LensButtonScript>();

    auto lens_icon = std::make_unique<Entity>("lens_icon");
    lens_icon->addComponent<TransformComp>(TransformComp(5,0));
    lens_icon->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {64,22,46,30}));

    lens_btn->addChild(std::move(lens_icon));
    
    inv_ui->addChild(std::move(inventory));
    inv_ui->addChild(std::move(bag));
    inv_ui->addChild(std::move(craft_btn));
    inv_ui->addChild(std::move(lens_btn));

#pragma endregion main_Inv_ui
    // adding subtitle UI
#pragma region subtitle
    auto sub_bg = std::make_unique<Entity>("subtitle_group");
    sub_bg->addComponent<TransformComp>(0,100);
    sub_bg->addComponent<DrawBg>(512,70);

    auto subtitle_text = std::make_unique<Entity>("subtitle");
    subtitle_text->addComponent<TransformComp>();
    subtitle_text->addComponent<DrawTextComp>(10,10,10);

    sub_bg->addChild(std::move(subtitle_text));

    ui.push_back(std::move(sub_bg));
    ui.push_back(std::move(inv_ui));

    //std::cout << ui[0].get()->name <<std::endl;
    main_inv_ui = getUI("ui_parent");

    subtitle = getUI("subtitle_group");
    auto s = getUI("subtitle");

    if(s)
        sub_text = s->getComponent<DrawTextComp>();
#pragma endregion subtitle

    auto crafting_ui = std::make_unique<Entity>("crafting_base");
    crafting_ui->addComponent<TransformComp>(350, 50);

    crafting_ui->addComponent<Panel_Sprite>(Panel_Sprite( 3.0f,{0,0,150,150})); // values are multiplying with camera zoom value

    crafting_ui->addComponent<Sprite>(Sprite( ResourcesLoader::ui_page, {182,0,30,30}));
    crafting_ui->addComponent<ColliderComp>();
    auto main_craft = crafting_ui->addComponent<Crafting_main>();
    main_craft->cr_slots.clear();

    for(int i = 0; i < 3; i++)
    {
        auto cr_slot = std::make_unique<Entity>("slot"+i);
        cr_slot->addComponent<TransformComp>(20 + i*40,40);
        cr_slot->addComponent<ColliderComp>(0,0,30,30);
        cr_slot->addComponent<Sprite>(Sprite( ResourcesLoader::ui_page, {212,0,30,30}));

        auto cr_slot_script = cr_slot->addComponent<Crafting_Slot>();
        cr_slot_script->idx = i;
        main_craft->cr_slots.push_back( std::move(cr_slot_script));
        
        auto cr_slot_icon = std::make_unique<Entity>("slot_icon");
        cr_slot_icon->addComponent<TransformComp>(3,3);
        cr_slot_icon->addComponent<Sprite>(Sprite(ResourcesLoader::inv_items, {0,0,24,24}));
        cr_slot_icon->setActive(false);
        cr_slot->addChild(std::move(cr_slot_icon));
        crafting_ui->addChild(std::move(cr_slot));

    }

    auto result_slot = std::make_unique<Entity>("result_slot");
    result_slot->addComponent<TransformComp>(60,100);
    result_slot->addComponent<Sprite>(Sprite( ResourcesLoader::ui_page, {212,0,30,30}));
    result_slot->addComponent<ColliderComp>();
    auto r_slot_script = result_slot->addComponent<Result_SLot>();
    main_craft->r_slot = r_slot_script;

    auto r_slot_icon = std::make_unique<Entity>("result_slot_icon");
    r_slot_icon->addComponent<TransformComp>(3,3);
    r_slot_icon->addComponent<Sprite>(Sprite(ResourcesLoader::inv_items, {0,0,24,24}));
    r_slot_icon->setActive(false);
    result_slot->addChild(std::move(r_slot_icon));

    crafting_ui->addChild(std::move(result_slot));


    auto title_craft = std::make_unique<Entity>("title_craft");
    title_craft->addComponent<TransformComp>(60,10);
    title_craft->addComponent<TextComp>("Crafting", 5, Fade(WHITE, 0.85));
    crafting_ui->addChild(std::move(title_craft));

    //addUI(std::move(result_slot));

    auto debug_btn = std::make_unique<Entity>("debug");
    debug_btn->addComponent<TransformComp>(0,0);
    debug_btn->addComponent<Panel_Sprite>(Panel_Sprite(3.0f, {0,0,30,15}));
    debug_btn->addComponent<Sprite>(Sprite( ResourcesLoader::ui_page, {212,0,30,30}));
    debug_btn->addComponent<TextComp>(TextComp("Debug", 5, {4,4},WHITE));
    debug_btn->addComponent<ColliderComp>();
    debug_btn->addComponent<OnDebug>();

    addUI(std::move(debug_btn));

    addUI(std::move(crafting_ui));

    main_inv_ui->setActive(false);
    subtitle->setActive(false);

    panel_ui= getUI("crafting_base");

    craft_man = panel_ui->getComponent<Crafting_main>();

    panel_ui->setActive(false);
}

void UI_Manager::UPdateUI()
 {


    for(auto s:inv_man->slots)
    {
        s->slot_item.id = 0;
        s->ClearSlots();
    }

    for(auto s:craft_man->cr_slots)
    {
        s->item.id = 0;
        s->ClearSlots();
    }

    for (int i = 0; i < Game::get_Instance().runtime_inv.size(); i++)
    {
        /* code */
        if(inv_man->slots.size() < i)break;

        if(inv_man->slots[i]){

            auto s = inv_man->slots[i];
            s->slot_item = Game::get_Instance().runtime_inv[i];
            s->UpdateSlot();
            if(s->slot_item.id < 0)
            {
                //std::cout << s->slot_item.name <<std::endl;
            }
            
        }


    }

    for(int i = 0; i < Game::get_Instance().crafting_inv.size(); i++)
    {
        if(craft_man->cr_slots.size() < i)break;

        if(craft_man->cr_slots[i])
        {
            auto s = craft_man->cr_slots[i];
            s->item = Game::get_Instance().crafting_inv[i];
            s->UpdateSlots();
        }
    }
    
}

void UI_Manager::ShowSubtitle(std::string sub, float t)

{
    subtitle->setActive(true);
    subtitle->getChild(0)->getComponent<DrawTextComp>()->SetText(sub, t);

    float delay = static_cast<float>(sub.length())* 0.15f;

    Game::get_Instance().schedular.Schedule(delay,[this]()
    {
        //Game::get_Instance().ui_m->subtitle->setActive(false);
        subtitle->setActive(false);
        
    }, 1000);
}

void UI_Manager::Update(float dt)
{   

    Game::get_Instance().IsOverUI = false;

    for(int i = ui.size() - 1; i >= 0; --i)
    {
        if(!ui[i]->isActive())continue;

        ui[i]->Update(dt);
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
           
            if(ui[i]->IsUnderMouse()){
                Game::get_Instance().IsOverUI = true;
                
                
                break;
            }
        }
    }

    // for(auto& e:ui)
    // {
    //     e->Update(dt);
    // }   
 
}

void UI_Manager::Draw()
{
    BeginMode2D(ui_cam);
    for(auto& e:ui)
    {
        e->Draw();
    }
    EndMode2D();
}

Entity *UI_Manager::getUI(std::string name)
{
    for(auto& u:ui)
    {
        if(u->name == name)
        {
            return u.get();
        }
    }
    return nullptr;
}

void UI_Manager::addUI(std::unique_ptr<Entity> ui)
{
    this->ui.push_back(std::move(ui));
}
