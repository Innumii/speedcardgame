#ifndef SHOP_PACKAGE_HPP
#define SHOP_PACKAGE_HPP

#include <string>
#include <vector>

class ShopPackage {
public:
    struct Package {
        std::string id;
        std::string name;
        int coins{0};
        int amountCents{0};
        std::string currency;
        int discountPercent{0};
    };

    static bool loadPackagesFromService();
    static const std::vector<Package>& getPackages();
    static int getNumberOfPackages();
};

#endif
