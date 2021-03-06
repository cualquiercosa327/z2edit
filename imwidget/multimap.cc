#include "imwidget/multimap.h"

#include "imwidget/imapp.h"
#include "imwidget/map_command.h"
#include "imwidget/simplemap.h"
#include "util/config.h"
#include "absl/strings/str_cat.h"
#include "alg/palace_gen.h"

#include <gflags/gflags.h>

DEFINE_bool(town_hack, true, "World 2 towns are really in world 1");

namespace z2util {

float MultiMap::xs_ = 0.40;
float MultiMap::ys_ = 0.75;
bool MultiMap::preconverge_ = true;
bool MultiMap::continuous_converge_ = true;

MultiMap* MultiMap::Spawn(Mapper* m, int world, int overworld, int subworld,
                          int map) {
    MultiMap* mm = new MultiMap(m, world, overworld, subworld, map);
    mm->Init();
    ImApp::Get()->AddDrawCallback(mm);
    return mm;
}

void MultiMap::Init() {
    const auto& ri = ConfigLoader<RomInfo>::GetConfig();

    if (FLAGS_town_hack) {
        if (world_ == 2) world_ = 1;
    }
    visited_room0_ = 0;
    int n = 0;
    for(const auto& m : ri.map()) {
        if (m.type() != MapType::OVERWORLD
            && m.world() == world_
            && m.overworld() == overworld_
            // only care about the subworld in world 0 (overworlds)
            && (world_ || m.subworld() == subworld_)) {
            maps_[n] = m;
            visited_[n] = false;
            n++;
        }
    }
    location_.clear();
    graph_.Clear();

    title_ = absl::StrCat("MultiMap: ", maps_[start_].name(), "##", id_);
    if (start_ != 0) {
        // Room 0 is often used as the destination for illegal exits
        visited_[0] = true;
    }
    Traverse(start_, 0, 0, -1);
    if (start_ != 0 && visited_room0_) {
        auto* node = AddRoom(0, 0, 4);
        node->set_charge(0.001);
    }

    if (preconverge_) {
        for(int i=0; i<10000; i++) {
            graph_.Compute(1.0/60.0);
        }
    }

    if (pgo_.grid_width() == 0) pgo_.set_grid_width(8);
    if (pgo_.grid_height() == 0) pgo_.set_grid_height(8);
    if (pgo_.num_rooms() == 0) pgo_.set_num_rooms(14);
    pgo_.set_start_room(start_);
    pgo_.set_world(world_);
}

fdg::Node* MultiMap::AddRoom(int room, int x, int y) {
    SimpleMap simple(mapper_, maps_[room]);
    auto buffer(simple.RenderToNewBuffer());
    buffer->Update();

    double xx = double(x) + room / 1000.0;
    double yy = double(y) + room / 1000.0;
    fdg::Node *node = graph_.AddNode(room, Vec2(xx, yy));

    location_.emplace(std::make_pair(
                room, DrawLocation{node, std::move(buffer)}));
    return node;
}

void MultiMap::Traverse(int room, int x, int y, int from) {
    if (room == 0)
        visited_room0_++;
    if (room == 63 || visited_[room])
        return;
    visited_[room] = true;

    auto* node = AddRoom(room, x, y);
    MapConnection conn;
    conn.set_mapper(mapper_);
    conn.Parse(maps_[room]);

    double k, w;
    int d;
    uint32_t col;
    auto* spring = node->mutable_connection();

    // yellow
    d = conn.left().destination;
    //k = d ? 1 : 0.001;
    k = (d || d == from) ? 1 : 0.001;
    col = (k < 1.0) ? GRAY : YELLOW;
    w   = (k < 1.0) ? 1 : 3;
    spring->emplace_back(fdg::Spring{d, k, fdg::Bias::Horizontal, col, w});

    // blue
    d = conn.down().destination;
    k = (d || d == from) ? 1 : 0.001;
    col = (k < 1.0) ? GRAY : BLUE;
    w   = (k < 1.0) ? 1 : 6;
    spring->emplace_back(fdg::Spring{d, k, fdg::Bias::Vertical, col, w});

    // green
    d = conn.up().destination;
    k = (d || d == from) ? 1 : 0.001;
    col = (k < 1.0) ? GRAY : GREEN;
    w   = (k < 1.0) ? 1 : 3;
    spring->emplace_back(fdg::Spring{d, k, fdg::Bias::Vertical, col, w});

    // red
    d = conn.right().destination;
    k = (d || d == from) ? 1 : 0.001;
    col = (k < 1.0) ? GRAY : RED;
    w   = (k < 1.0) ? 1 : 6;
    spring->emplace_back(fdg::Spring{d, k, fdg::Bias::Horizontal, col, w});

    Traverse(conn.left().destination,  x-1, y, room);
    Traverse(conn.down().destination,  x, y+1, room);
    Traverse(conn.up().destination,    x, y-1, room);
    Traverse(conn.right().destination, x+1, y, room);
}

void MultiMap::Sort() {
}

Vec2 MultiMap::Position(const Vec2& pos) {
    float width = 1024.0 * scale_; //+ 8.0;
    float height = 224.0 * scale_; //+ 32.0;
    return Vec2(pos.x * xs_ * width, pos.y * ys_ * height);
}

Vec2 MultiMap::Position(const DrawLocation& dl, Direction side) {
    float w = dl.buffer->width() * scale_;
    float h = dl.buffer->height() * scale_;
    Vec2 pos = Position(dl.node->pos()) + Vec2(0, 24);
    switch(side) {
        case LEFT:  pos += Vec2(0, h/2.0); break;
        case DOWN:  pos += Vec2(w/2.0, h); break;
        case UP:    pos += Vec2(w/2.0, 0); break;
        case RIGHT: pos += Vec2(w, h/2.0); break;
        default:
            pos += Vec2(w/2.0, h/2.0);
    }
    return pos;
}

void MultiMap::DrawArrow(const Vec2& a, const Vec2&b, uint32_t color,
                         float width, float arrowpos, float rootsize) {
    if (width == 0) width = 2.0f;
    Vec2 u = (b - a).unit();
    Vec2 v = u.flip();
    Vec2 p = a + u * ((b - a).length() * arrowpos);

    auto* draw = ImGui::GetWindowDrawList();
    draw->AddTriangleFilled(p-v*10.0, p+v*10.0, p+u*20.0, color);
    draw->AddCircleFilled(a, rootsize, color);
    draw->AddLine(a, b, color, width);
}

void MultiMap::DrawConnections(const DrawLocation& dl) {
    Direction side = NONE;
    for(const auto& spring : dl.node->connection()) {
        side = Direction(int(side) + 1);
        const auto& loc = location_.find(spring.destid);
        if (loc == location_.end())
            continue;

        Vec2 a = absolute_ + Position(dl, side);
        Vec2 b = absolute_ + Position(loc->second, NONE);
        DrawArrow(a, b, spring.color, spring.width, 0.1, 10.0);
    }
}

void MultiMap::DrawOne(const DrawLocation& dl) {
    int map = dl.node->id();
    Vec2 pos = origin_ + Position(dl.node->pos());
    Vec2 button_height(0, 24);
    ImGui::SetCursorPos(pos);
    if (show_labels_ && ImGui::Button(maps_[dl.node->id()].name().c_str())) {
        SimpleMap::Spawn(mapper_, maps_[map]);
    }
    pos += button_height;
    ImGui::SetCursorPos(pos);
    ImGui::InvisibleButton(maps_[dl.node->id()].name().c_str(),
                           ImVec2(dl.buffer->width() * scale_,
                                  dl.buffer->height() * scale_));
    dl.node->set_pause(false);
    if (ImGui::IsItemActive()) {
        drag_ |= true;
        if (ImGui::IsMouseDragging()) {
            Vec2 delta = Vec2(ImGui::GetIO().MouseDelta.x / (1024.0 * xs_ * scale_),
                              ImGui::GetIO().MouseDelta.y / (224.0 * ys_ * scale_));
            dl.node->set_pos(dl.node->pos() + delta);
            dl.node->set_pause(true);
        }
    }
    dl.buffer->DrawAt(pos.x, pos.y, scale_);
}

void MultiMap::DrawLegend() {
    if (ImGui::BeginPopup("Legend")) {
        ImGui::Text("Legend:");
        auto p = ImGui::GetCursorScreenPos();
        ImGui::Text("Right Exit:                    ");
        DrawArrow(Vec2(100, 8)+p, Vec2(200, 8)+p, RED);

        p = ImGui::GetCursorScreenPos();
        ImGui::Text("Left Exit: ");
        DrawArrow(Vec2(200, 8)+p, Vec2(100, 8)+p, YELLOW);

        p = ImGui::GetCursorScreenPos();
        ImGui::Text("Up Exit:   ");
        DrawArrow(Vec2(100, 8)+p, Vec2(200, 8)+p, GREEN);

        p = ImGui::GetCursorScreenPos();
        ImGui::Text("Down Exit: ");
        DrawArrow(Vec2(200, 8)+p, Vec2(100, 8)+p, BLUE);

        p = ImGui::GetCursorScreenPos();
        ImGui::Text("Illegal Exit:");
        DrawArrow(Vec2(100, 8)+p, Vec2(200, 8)+p, GRAY);

        ImGui::EndPopup();
    }
}

void MultiMap::DrawGen() {
    if (ImGui::BeginPopup("Generate")) {
        int seed = pgo_.seed();
        if (ImGui::InputInt("Seed", &seed)) { pgo_.set_seed(seed); }
        int w = pgo_.grid_width();
        if (ImGui::InputInt("Width", &w)) {  pgo_.set_grid_width(w); }
        ImGui::SameLine();
        int h = pgo_.grid_height();
        if (ImGui::InputInt("Height", &h)) {  pgo_.set_grid_height(h); }

        int n = pgo_.num_rooms();
        if (ImGui::InputInt("Rooms", &n)) {  pgo_.set_num_rooms(n); }

        bool efr = pgo_.enter_on_first_row();
        if (ImGui::Checkbox("Enter on first row", &efr)) { 
            pgo_.set_enter_on_first_row(efr);
        }

        bool jump = pgo_.jump_required();
        if (ImGui::Checkbox("Jump required", &jump)) {
            pgo_.set_jump_required(jump);
        }
        bool glove = pgo_.glove_required();
        if (ImGui::Checkbox("Glove required", &glove)) {
            pgo_.set_glove_required(glove);
        }
        bool fairy = pgo_.fairy_required();
        if (ImGui::Checkbox("Fairy required", &fairy)) {
            pgo_.set_fairy_required(fairy);
        }



        if (ImGui::Button("Generate")) {
            PalaceGenerator pgen(pgo_);
            pgen.set_mapper(mapper_);
            pgen.Generate();
            Init();
        }
        ImGui::EndPopup();
    }
}

bool MultiMap::Draw() {
    if (!visible_)
        return false;

    drag_ = false;
    ImGui::SetNextWindowSize(ImVec2(1024, 700), ImGuiCond_FirstUseEver);
    ImGui::Begin(title_.c_str(), &visible_);
    ImGui::PushItemWidth(100);
    ImGui::InputFloat("Zoom", &scale_, 1.0/8.0, 1.0);
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        Init();
    }
    ImGui::SameLine();
    if (ImGui::Button("Properties")) {
        ImGui::OpenPopup("Properties");
    }
    if (ImGui::BeginPopup("Properties")) {
        ImGui::SliderFloat("X-Zoom", &xs_, 0.001f, 1.0f);
        ImGui::SliderFloat("Y-Zoom", &ys_, 0.001f, 1.0f);
        ImGui::Checkbox("Pause Convergence while dragging", &pauseconv_);
        ImGui::Checkbox("Converge before first draw", &preconverge_);
        ImGui::Checkbox("Converge during draw", &continuous_converge_);
        ImGui::Checkbox("Show labels", &show_labels_);
        ImGui::Checkbox("Show arrows", &show_arrows_);
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Legend")) {
        ImGui::OpenPopup("Legend");
    }
    DrawLegend();

    ImGui::SameLine();
    if (ImGui::Button("Generate")) {
        ImGui::OpenPopup("Generate");
    }
    DrawGen();

    ImGui::SameLine();
    ImApp::Get()->HelpButton("overworld-editor");

    Vec2 minv(1e9, 1e9);
    Vec2 maxv(-1e9, -1e9);
    for(const auto& dl : location_) {
        Vec2 p = dl.second.node->pos();
        minv.x = std::min(minv.x, p.x);
        minv.y = std::min(minv.y, p.y);
        maxv.x = std::max(maxv.x, p.x);
        maxv.y = std::max(maxv.y, p.y);
    }
    maxv += Vec2(1, 1);
    minv = Position(minv);
    maxv = Position(maxv);
    ImGui::BeginChild("image", ImVec2(0, 0), true,
                      ImGuiWindowFlags_AlwaysHorizontalScrollbar |
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    origin_ = -minv + ImGui::GetCursorPos();
    absolute_ = -minv + ImGui::GetCursorScreenPos();

    if (show_arrows_) {
        for(const auto& dl : location_)
            DrawConnections(dl.second);
    }
    for(const auto& dl : location_) {
        DrawOne(dl.second);
    }

    ImGui::EndChild();
    ImGui::End();

    if (continuous_converge_ && !(drag_ && pauseconv_))
        graph_.Compute(1.0/60.0);

    return false;
}

}  // namespace z2util
