#pragma once
namespace dvr::hudalpha {
struct Config { int mode; float gain,floorA,gamma,mixK; };
inline Config original() {return {0,1,0,1,1};}
enum Owner {General,Wheel,Reading,Interaction,Pause};
struct Bank {
    Config general=original(),special[4]={original(),original(),original(),original()};
    const Config& for_owner(Owner owner) const {return owner==General ? general : special[owner-1];}
    void reset_general() {general=original();}
};
}
