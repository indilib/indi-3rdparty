#include "enclosure_state.h"
#include <cassert>
#include <iostream>
int main(){
 using tenmicron::EnclosureState;
 EnclosureState e;
 assert(e.permits(true,false)); // outdoor defaults, no invented dome requirement
 assert(e.permits(true,true)); // explicit Ignore is honored
 assert(!e.permits(false,false)); // Locks without a device is an error
 assert(!e.permits(false,true)); // configured but not observed
 e.parkOpen=true;assert(!e.permits(false,true));
 e.connection(true);assert(!e.parkOpen); // previous transport's park is invalid
 e.parkOpen=true;assert(e.permits(false,true));
 e.connection(true);assert(e.permits(false,true)); // repeated connected event
 e.shutterSeen=true;assert(!e.permits(false,true));
 e.shutterOpen=true;assert(e.permits(false,true));
 e.parkOpen=false;assert(!e.permits(false,true));
 e.parkOpen=true;e.connection(false);assert(!e.permits(false,true));
 e.connection(true);assert(!e.permits(false,true));
 e.parkOpen=true;assert(!e.permits(false,true)); // require new shutter too
 e.shutterOpen=true;assert(e.permits(false,true));
 e.reset();assert(!e.permits(false,true));
 std::cout<<"16 standard enclosure policy assertions passed\n";
}
