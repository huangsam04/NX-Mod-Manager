                break;
        }
    }
        
        
    // 在右侧信息框绘制当前选中模组的详细信息（左对齐，顶部对齐）
    gfx::drawTextBoxCentered(this->vg, 
            icon_x ,                                 // X坐标
            icon_y + icon_size + 40.f + 14.f,             // Y坐标  
            sidebox_w - 70.f,                     // 宽度（减去左右边距）
            sidebox_h - icon_size - 35.f - 40.f - 21.f -22.f,         // 高度
            21.f,                                 // 字体大小
            1.5f,                                 // 行间距倍数
            MOD_TYPE_TEXT.c_str(),             // 当前选中模组的文本
            nullptr, 
            NVG_ALIGN_LEFT | NVG_ALIGN_TOP,       // 左对齐，顶部对齐
            gfx::Colour::WHITE
    );


    // 绘制左下角mod数量 (Draw mod count in bottom-left corner)
    gfx::drawTextArgs(this->vg, 55.f, 670.f, 24.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE, TOTAL_COUNT.c_str(), mod_info.size());

    // 恢复NanoVG绘图状态 / Restore NanoVG drawing state
    nvgRestore(this->vg);

    gfx::drawButtons(this->vg, 
            gfx::Colour::WHITE, 
            gfx::pair{gfx::Button::B, BUTTON_BACK.c_str()}, 
            gfx::pair{gfx::Button::A, BUTTON_SELECT.c_str()},
            gfx::pair{gfx::Button::X, CLEAR_BUTTON.c_str()}
        );

    // 绘制对话框（如果显示）(Draw dialog if shown)
    this->DrawDialog();

}

// 对比mod版本和游戏版本是否一致的辅助函数 (Helper function to compare mod version and game version consistency)
bool App::CompareModGameVersion(const std::string& mod_version, const std::string& game_version) {
    // 情况1：直接字符串匹配 (Case 1: Direct string match)
    if (mod_version == game_version) {
        return true;
    }
    
    // 情况2：简化版本号对比 - 去掉小数点和末尾0后对比 (Case 2: Simplified version comparison - remove dots and trailing zeros)
    auto simplify_version = [](const std::string& version) -> std::string {
        std::string simplified = version;
        
        // 移除所有空格 (Remove all spaces)
        simplified.erase(std::remove(simplified.begin(), simplified.end(), ' '), simplified.end());
        
        // 转换为小写 (Convert to lowercase)
        std::transform(simplified.begin(), simplified.end(), simplified.begin(), ::tolower);
        
        // 移除前缀v或V (Remove prefix v or V)
        if (!simplified.empty() && (simplified[0] == 'v')) {
            simplified = simplified.substr(1);
        }
        
        // 移除所有小数点 (Remove all dots)
        simplified.erase(std::remove(simplified.begin(), simplified.end(), '.'), simplified.end());
        
        // 移除末尾的所有0 (Remove all trailing zeros)
        while (!simplified.empty() && simplified.back() == '0') {
            simplified.pop_back();
        }
        
        // 如果结果为空，返回"0" (If result is empty, return "0")
        if (simplified.empty()) {
            simplified = "0";
        }
        
        return simplified;
    };
    
    std::string simple_mod = simplify_version(mod_version);
    std::string simple_game = simplify_version(game_version);
    
    // 简化后对比 (Compare after simplification)
    return simple_mod == simple_game;
}

void App::Sort()
{
    // std::scoped_lock lock{entries_mutex}; // 保护entries向量的排序操作 (Protect entries vector sorting operation)
    switch (static_cast<SortType>(this->sort_type)) {
        case SortType::Alphabetical_Reverse:
            // 按应用名称Z-A排序，但先按安装状态分组
            // Sort by app name Z-A, but group by installation status first
            std::ranges::sort(this->entries, [](const AppEntry& a, const AppEntry& b) {
                // 首先按安装状态分组：已安装的在前，未安装的在后
                // First group by installation status: installed first, uninstalled last
                bool a_installed = (a.display_version != NONE_GAME_TEXT);
                bool b_installed = (b.display_version != NONE_GAME_TEXT);
                
                if (a_installed != b_installed) {
                    return a_installed > b_installed; // 已安装的在前 (installed first)
                }
                
                // 在同一组内按字母顺序Z-A排序
                // Within the same group, sort alphabetically Z-A
                return a.FILE_NAME2 > b.FILE_NAME2;
            });
            break;
        case SortType::Alphabetical:
            // 按应用名称A-Z排序，但先按安装状态分组
            // Sort by app name A-Z, but group by installation status first
            std::ranges::sort(this->entries, [](const AppEntry& a, const AppEntry& b) {
                // 首先按安装状态分组：已安装的在前，未安装的在后
                // First group by installation status: installed first, uninstalled last
                bool a_installed = (a.display_version != NONE_GAME_TEXT);
                bool b_installed = (b.display_version != NONE_GAME_TEXT);
                
                if (a_installed != b_installed) {
                    return a_installed > b_installed; // 已安装的在前 (installed first)
                }
                
                // 在同一组内按字母顺序A-Z排序
                // Within the same group, sort alphabetically A-Z
                return a.FILE_NAME2 < b.FILE_NAME2;
            });
            break;
        default:
            // 默认按应用名称A-Z排序，但先按安装状态分组
            // Default sort by app name A-Z, but group by installation status first
            std::ranges::sort(this->entries, [](const AppEntry& a, const AppEntry& b) {
                // 首先按安装状态分组：已安装的在前，未安装的在后
                // First group by installation status: installed first, uninstalled last
                bool a_installed = (a.display_version != NONE_GAME_TEXT);
                bool b_installed = (b.display_version != NONE_GAME_TEXT);
                
                if (a_installed != b_installed) {
                    return a_installed > b_installed; // 已安装的在前 (installed first)
                }
                
                // 在同一组内按字母顺序A-Z排序
                // Within the same group, sort alphabetically A-Z
                return a.FILE_NAME2 < b.FILE_NAME2;
            });
            break;
    }
}

const char* App::GetSortStr() {
    switch (static_cast<SortType>(this->sort_type)) {
        case SortType::Alphabetical_Reverse:
            // 返回按字母顺序逆排序的提示文本
            return SORT_ALPHA_ZA.c_str();
        case SortType::Alphabetical:
            // 返回按字母顺序排序的提示文本
            return SORT_ALPHA_AZ.c_str();
        default:
            // 默认返回按容量从大到小排序的提示文本
            return SORT_ALPHA_AZ.c_str();
    }
}

void App::UpdateLoad() {
    if (this->controller.B) {
        this->async_thread.request_stop();
        this->async_thread.get();
        this->quit = true;
        return;
    }

    {
        std::scoped_lock lock{this->mutex};
        // 只要有应用加载完成就立即显示列表，实现真正的立即显示
        // Show list immediately when any app is loaded, achieving true immediate display
        if (scanned_count.load() > 0 || finished_scanning) {
            if (finished_scanning) {
                this->async_thread.get();
            }
            
            
            this->menu_mode = MenuMode::LIST;
        }
    }
}

void App::UpdateList() {
    // 如果对话框显示，优先处理对话框输入 (If dialog is shown, prioritize dialog input)
    if (this->show_dialog) {
        this->UpdateDialog();
        return; // 对话框显示时不处理列表输入 (Don't process list input when dialog is shown)
    }
    
    std::scoped_lock lock{entries_mutex}; // 保护entries向量的访问和修改 (Protect entries vector access and modification)
    // 检查应用列表是否为空，避免数组越界访问 (Check if application list is empty to avoid array out-of-bounds access)
    if (this->entries.empty()) {
        // 如果应用列表为空，只处理退出操作 (If application list is empty, only handle exit operation)
        if (this->controller.B) {
            this->audio_manager.PlayKeySound(0.9);
            this->quit = true;
        }
        return; // 提前返回，避免后续操作 (Early return to avoid subsequent operations)
    }
    
    // 扫描过程中禁用排序和删除功能
    if (this->controller.B) {
        this->audio_manager.PlayKeySound(0.9);
        this->quit = true;
    } else if (this->controller.A) { // 允许在扫描过程中选择列表项
        // 重置MOD列表的索引变量 (Reset MOD list index variables)
        this->mod_index = 0;
        this->mod_start = 0;
        this->mod_yoff = 130.f;
        this->mod_ypos = 130.f;

        if (!mod_icons_loaded) {
            // 加载所有MOD图标 (Load all MOD icons)
            this->Cheat_code_MOD_image = nvgCreateImage(this->vg, "romfs:/Cheat_code_MOD.jpg", NVG_IMAGE_NEAREST);
            this->Cosmetic_MOD_image = nvgCreateImage(this->vg, "romfs:/Cosmetic_MOD.jpg", NVG_IMAGE_NEAREST);
            this->FPS_MOD_image = nvgCreateImage(this->vg, "romfs:/FPS_MOD.jpg", NVG_IMAGE_NEAREST);
            this->HD_MOD_image = nvgCreateImage(this->vg, "romfs:/HD_MOD.jpg", NVG_IMAGE_NEAREST);
            this->More_PLAY_MOD_image = nvgCreateImage(this->vg, "romfs:/More_PLAY_MOD.jpg", NVG_IMAGE_NEAREST);
            this->NONE_MOD_image = nvgCreateImage(this->vg, "romfs:/NONE_MOD.jpg", NVG_IMAGE_NEAREST);

            mod_icons_loaded = true; // 标记为已加载 (Mark as loaded)
        }
    
        // 智能缓存机制：只有当游戏发生变化时才重新加载映射数据 (Smart cache mechanism: only reload mapping data when game changes)
        const auto& current_game = this->entries[this->index];
        if (current_game_name != current_game.FILE_NAME) {
            LoadModNameMapping(current_game.FILE_NAME, current_game.FILE_PATH);
        }
        FastScanModInfo();
        Sort_Mod();
        this->audio_manager.PlayKeySound(0.9);
        this->menu_mode = MenuMode::MODLIST;
    } else if (this->controller.X) { // X键显示确认对话框 (X key to show confirmation dialog)
        
        this->ShowDialog(DialogType::INFO,
             "作者：TOM\n版本：1.0.0\nQ群：1051287661\n乡村大舞台，折腾你就来！",
              24.0f,NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);

        this->audio_manager.PlayConfirmSound(); // 播放确认音效 (Play confirm sound effect)
        
    } else if (this->controller.START) { // +键显示使用说明界面
        
        this->audio_manager.PlayKeySound(0.9);
        this->menu_mode = MenuMode::INSTRUCTION;
        
    } else if (this->controller.DOWN) { // move down
        if (this->index < (this->entries.size() - 1)) {
            this->audio_manager.PlayKeySound(0.9);
            this->index++;
            this->ypos += this->BOX_HEIGHT;
            if ((this->ypos + this->BOX_HEIGHT) > 646.f) {
                LOG("moved down\n");
                this->ypos -= this->BOX_HEIGHT;
                this->yoff = this->ypos - ((this->index - this->start - 1) * this->BOX_HEIGHT);
                this->start++;
            }
            // 光标移动后的图标加载由每帧调用自动处理
            // Icon loading after cursor movement is handled by per-frame calls
        }else this->audio_manager.PlayLimitSound(1.5); 
    } else if (this->controller.UP) { // move up
        if (this->index != 0 && this->entries.size()) {
            this->audio_manager.PlayKeySound(0.9);
            this->index--;
            this->ypos -= this->BOX_HEIGHT;
            if (this->ypos < 86.f) {
                LOG("moved up\n");
                this->ypos += this->BOX_HEIGHT;
                this->yoff = this->ypos;
                this->start--;
            }// 播放按键音效 (Play key sound)
            // 光标移动后触发视口感知图标加载
            // 光标移动后的图标加载由每帧调用自动处理
            // Icon loading after cursor movement is handled by per-frame calls
        }else this->audio_manager.PlayLimitSound(1.5); 
    } else if (!is_scan_running && this->controller.Y) { // 非扫描状态下才允许排序
        this->audio_manager.PlayKeySound(); // 播放按键音效 (Play key sound)
        this->sort_type++;

        if (this->sort_type == std::to_underlying(SortType::MAX)) {
            this->sort_type = 0;
        }

        this->Sort();
        
        // 强制重置可见区域缓存，确保排序后图标能重新加载 (Force reset visible range cache to ensure icons reload after sorting)
        this->last_loaded_range = {SIZE_MAX, SIZE_MAX};
        
        // 重置选择索引，使选择框回到第一项
        this->index = 0;
        // 重置滚动位置
        this->ypos = this->yoff = 130.f;
        this->start = 0;

    } else if (this->controller.L) { // L键向上翻页 (L key for page up)
        if (this->entries.size() > 0) {
            // 直接更新index，向上翻页4个位置 (Directly update index, page up 4 positions)
            if (this->index >= 4) {
                this->audio_manager.PlayKeySound(0.9); // 正常翻页音效 (Normal page flip sound)
                this->index -= 4;
            } else {
                this->index = 0;
                this->audio_manager.PlayLimitSound(1.5);
            }
            
            // 直接更新start位置，确保翻页显示4个项目 (Directly update start position to ensure 4 items per page)
            if (this->start >= 4) {
                this->start -= 4;
            } else {
                this->start = 0;
            }
            
            // 边界检查：确保index和start的一致性 (Boundary check: ensure consistency between index and start)
            if (this->index < this->start) {
                this->index = this->start;
            }
            
            // 边界检查：确保index不会超出当前页面范围 (Boundary check: ensure index doesn't exceed current page range)
            std::size_t max_index_in_page = this->start + 3; // 当前页面最大索引 (Maximum index in current page)
            if (this->index > max_index_in_page && max_index_in_page < this->entries.size()) {
                this->index = max_index_in_page;
            }
            
            // 更新相关显示变量 (Update related display variables)
            this->ypos = 130.f + (this->index - this->start) * this->BOX_HEIGHT;
            this->yoff = 130.f;
            
            // 翻页后的图标加载由每帧调用自动处理 (Icon loading after page change is handled by per-frame calls)
        }
    } else if (this->controller.R) { // R键向下翻页 (R key for page down)
        if (this->entries.size() > 0) {
            // 直接更新index，向下翻页4个位置 (Directly update index, page down 4 positions)
            this->index += 4;
            
            // 边界检查：确保不超出列表范围 (Boundary check: ensure not exceeding list range)
            if (this->index >= this->entries.size()) {
                this->index = this->entries.size() - 1;
                this->audio_manager.PlayLimitSound(1.5);
            }else this->audio_manager.PlayKeySound(0.9);
            
            // 直接更新start位置，确保翻页显示4个项目 (Directly update start position to ensure 4 items per page)
            this->start += 4;
            
            // 边界检查：确保start不会超出合理范围 (Boundary check: ensure start doesn't exceed reasonable range)
            if (this->entries.size() > 4) {
                std::size_t max_start = this->entries.size() - 4;
                if (this->start > max_start) {
                    this->start = max_start;
                    // 当到达末尾时，调整index到最后一个可见项 (When reaching end, adjust index to last visible item)
                    this->index = this->entries.size() - 1;
                }
            } else {
                this->start = 0;
            }
            
            // 更新相关显示变量 (Update related display variables)
            this->ypos = 130.f + (this->index - this->start) * this->BOX_HEIGHT;
            this->yoff = 130.f;
            
            // 翻页后的图标加载由每帧调用自动处理 (Icon loading after page change is handled by per-frame calls)
        }
    } 

    
    // handle direction keys
}

void App::UpdateModList() {
    // 如果对话框显示，优先处理对话框输入 (If dialog is shown, prioritize dialog input)
    if (this->show_dialog) {
        this->UpdateDialog();
        return; // 对话框显示时不处理列表输入 (Don't process list input when dialog is shown)
    }
    
    std::string select_mod_name;
    bool select_mod_state;



    // 如果当前游戏没有MOD，只处理返回操作 (If current game has no MODs, only handle back operation)
    if (mod_info.empty()) {
        if (this->controller.B) {
            this->audio_manager.PlayKeySound(0.9);
            this->menu_mode = MenuMode::LIST;
        }
        return;
    }else {
        
        select_mod_name = this->mod_info[this->mod_index].MOD_NAME2;
        select_mod_state = this->mod_info[this->mod_index].MOD_STATE;

    }
    
    if (this->controller.B) {

        this->audio_manager.PlayKeySound(0.9);
        this->menu_mode = MenuMode::LIST;
    } else if (this->controller.A) {

        if (select_mod_state) this->ShowDialog(DialogType::CONFIRM,GetSnprintf(CONFIRM_UNINSTALLED,select_mod_name),26.0f,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
        else this->ShowDialog(DialogType::CONFIRM,GetSnprintf(CONFIRM_INSTALLED,select_mod_name),26.0f,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
        

        this->audio_manager.PlayConfirmSound();

    } else if (this->controller.X) { // X键清理缓存 (X key to clear cache)

        this->clean_button = true;
        this->ShowDialog(DialogType::CONFIRM,GetSnprintf(CONFIRM_UNINSTALLED,select_mod_name),26.0f,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);

    } else if (this->controller.DOWN) { // 向下移动 (Move down)
        if (this->mod_index < (mod_info.size() - 1)) {
            this->audio_manager.PlayKeySound(0.9);
            this->mod_index++;
            this->mod_ypos += this->BOX_HEIGHT;
            if ((this->mod_ypos + this->BOX_HEIGHT) > 646.f) {
                this->mod_ypos -= this->BOX_HEIGHT;
                this->mod_yoff = this->mod_ypos - ((this->mod_index - this->mod_start - 1) * this->BOX_HEIGHT);
                this->mod_start++;
            }
        } else {
            this->audio_manager.PlayLimitSound(1.5);
        }
    } else if (this->controller.UP) { // 向上移动 (Move up)
        if (this->mod_index != 0 && mod_info.size()) {
            this->audio_manager.PlayKeySound(0.9);
            this->mod_index--;
            this->mod_ypos -= this->BOX_HEIGHT;
            if (this->mod_ypos < 86.f) {
                this->mod_ypos += this->BOX_HEIGHT;
                this->mod_yoff = this->mod_ypos;
                this->mod_start--;
            }
        } else {
            this->audio_manager.PlayLimitSound(1.5);
        }
    } else if (this->controller.L) { // L键向上翻页 (L key for page up)
        if (mod_info.size() > 0) {
            // 直接更新index，向上翻页4个位置 (Directly update index, page up 4 positions)
            if (this->mod_index >= 4) {